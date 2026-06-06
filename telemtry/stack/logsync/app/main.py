"""HTTP surface for the logsync worker (FastAPI).

The viewer proxies the browser to these endpoints. Everything is JSON except
the SSE progress stream and the file/archive downloads.
"""
from __future__ import annotations

import asyncio
import csv
import io
import json
import os
import time
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, StreamingResponse
from pydantic import BaseModel, ConfigDict

from .config import config
from .manager import JobManager
from .models import QUALITY_VALUES, Annotation
from .pi import list_remote_logs, select_in_range
from .store import AnnotationStore, SessionStore

manager: JobManager | None = None
annotations: AnnotationStore | None = None
sessions: SessionStore | None = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global manager, annotations, sessions
    os.makedirs(config.staging_dir, exist_ok=True)
    annotations = AnnotationStore(config.state_db)
    sessions = SessionStore(config.state_db)
    manager = JobManager()
    await manager.start()
    yield
    await manager.stop()


app = FastAPI(title="BEVO logsync", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)


def _mgr() -> JobManager:
    if manager is None:
        raise HTTPException(503, "worker not ready")
    return manager


def _anno() -> AnnotationStore:
    if annotations is None:
        raise HTTPException(503, "worker not ready")
    return annotations


def _sess() -> SessionStore:
    if sessions is None:
        raise HTTPException(503, "worker not ready")
    return sessions


class CreateJobBody(BaseModel):
    from_ms: int
    to_ms: int
    bwlimit_kbps: int | None = None


class AnnotationBody(BaseModel):
    """Editable annotation fields. All optional; omitted fields reset to blank
    (the client always sends the full form), so a PUT fully replaces the row."""
    notes: str = ""
    tags: list[str] = []
    driver: str = ""
    track: str = ""
    session: str = ""
    weather: str = ""
    tires: str = ""
    setup: str = ""
    starred: bool = False
    quality: str = ""


class SessionBody(BaseModel):
    """A trackside session record from Trackside Live, stored verbatim so the
    annotation UI can match a log CSV's loggerd timestamp into the window. Mirror
    of the viewer's TracksideSessionInfo (camelCase kept on purpose).

    extra='allow' so the FULL session record — the rich metadata (vehicle id /
    weight / type / comments, short+long notes) and the energy plan (targetLaps,
    targetEnergyKwh, soeCutoffCellV) — is persisted verbatim in the JSON blob,
    not just the typed summary fields. No DB migration: the store keeps `data`
    as JSON."""
    model_config = ConfigDict(extra="allow")
    id: str
    name: str = ""
    car: str = ""
    driver: str = ""
    eventType: str = ""
    venue: str = ""
    startedAt: int = 0
    endedAt: int | None = None
    laps: int = 0


@app.get("/health")
async def health():
    return {"ok": True, "ssh_target": config.ssh_target, "remote_log_dir": config.remote_log_dir}


@app.get("/motion")
async def motion():
    return (await _mgr().motion.read()).to_dict()


@app.get("/logs")
async def preview_logs(from_ms: int, to_ms: int):
    """Dry-run: which files (and how many bytes) a job for this window would pull."""
    try:
        files = select_in_range(await list_remote_logs(), from_ms, to_ms)
    except Exception as e:
        raise HTTPException(502, f"could not list Pi logs: {e}")
    return {
        "from_ms": from_ms,
        "to_ms": to_ms,
        "count": len(files),
        "total_bytes": sum(f.size for f in files),
        "files": [f.to_dict() for f in files],
    }


@app.post("/jobs")
async def create_job(body: CreateJobBody):
    try:
        files = select_in_range(await list_remote_logs(), body.from_ms, body.to_ms)
    except Exception as e:
        raise HTTPException(502, f"could not list Pi logs: {e}")
    if not files:
        raise HTTPException(400, "no log files overlap the requested time range")
    job = await _mgr().create_job(body.from_ms, body.to_ms, files, body.bwlimit_kbps)
    return job.to_dict()


@app.get("/jobs")
async def list_jobs():
    return [j.to_dict() for j in _mgr().list()]


# ----- file annotations (trackside notes, keyed by filename) -----


@app.get("/annotations")
async def list_annotations():
    """All annotations as a {name: annotation} map, for the viewer to merge
    into its file list in one fetch."""
    return {a.name: a.to_dict() for a in _anno().list()}


@app.get("/annotations/{name}")
async def get_annotation(name: str):
    ann = _anno().get(name)
    return ann.to_dict() if ann else Annotation(name=name).to_dict()


@app.put("/annotations/{name}")
async def put_annotation(name: str, body: AnnotationBody):
    if body.quality not in QUALITY_VALUES:
        raise HTTPException(422, f"quality must be one of {QUALITY_VALUES}")
    store = _anno()
    ann = Annotation(
        name=name,
        notes=body.notes.strip(),
        # de-dupe, drop blanks, preserve order
        tags=list(dict.fromkeys(t.strip() for t in body.tags if t.strip())),
        driver=body.driver.strip(),
        track=body.track.strip(),
        session=body.session.strip(),
        weather=body.weather.strip(),
        tires=body.tires.strip(),
        setup=body.setup.strip(),
        starred=body.starred,
        quality=body.quality,
        updated_ms=int(time.time() * 1000),
    )
    # An annotation cleared back to empty is deleted rather than stored blank,
    # so "has notes?" stays a simple presence check on the viewer side.
    if ann.is_empty():
        store.delete(name)
        return Annotation(name=name).to_dict()
    store.upsert(ann)
    return ann.to_dict()


# ----- trackside sessions (named run windows, for cross-device CSV matching) -----


@app.get("/sessions")
async def list_sessions():
    """All trackside sessions as an {id: session} map, for the annotation UI to
    match a log CSV's timestamp against from any device."""
    return {s["id"]: s for s in _sess().list() if "id" in s}


@app.get("/sessions/{sid}")
async def get_session(sid: str):
    s = _sess().get(sid)
    if not s:
        raise HTTPException(404, "session not found")
    return s


@app.post("/sessions")
async def upsert_session(body: SessionBody):
    data = body.model_dump()
    data["updated_ms"] = int(time.time() * 1000)
    _sess().upsert(body.id, data)
    return data


def _require_job(job_id: str):
    job = _mgr().get(job_id)
    if not job:
        raise HTTPException(404, "job not found")
    return job


@app.get("/jobs/{job_id}")
async def get_job(job_id: str):
    return _require_job(job_id).to_dict()


@app.post("/jobs/{job_id}/pause")
async def pause_job(job_id: str):
    _require_job(job_id)
    if not _mgr().pause(job_id):
        raise HTTPException(409, "job cannot be paused in its current state")
    return _require_job(job_id).to_dict()


@app.post("/jobs/{job_id}/resume")
async def resume_job(job_id: str):
    _require_job(job_id)
    if not _mgr().resume(job_id):
        raise HTTPException(409, "job cannot be resumed in its current state")
    return _require_job(job_id).to_dict()


@app.post("/jobs/{job_id}/cancel")
async def cancel_job(job_id: str):
    _require_job(job_id)
    if not _mgr().cancel(job_id):
        raise HTTPException(409, "job cannot be canceled in its current state")
    return _require_job(job_id).to_dict()


def _safe_file(job, name: str) -> str:
    """Resolve a requested filename to a path in the shared store, after
    confirming the name belongs to this job."""
    if name not in {f.name for f in job.files}:
        raise HTTPException(404, "file not part of this job")
    dest = _mgr().store_dir
    path = os.path.realpath(os.path.join(dest, name))
    if not path.startswith(os.path.realpath(dest) + os.sep):
        raise HTTPException(400, "invalid filename")
    if not os.path.exists(path):
        raise HTTPException(409, "file not transferred yet")
    return path


@app.get("/jobs/{job_id}/files/{name}/head")
async def file_head(job_id: str, name: str, rows: int = 10):
    """First `rows` data rows + the header, so the viewer can preview a file and
    let the user choose columns before downloading. Reads only the head — the
    csv reader stops early, so this is cheap even on multi-GB files."""
    job = _require_job(job_id)
    path = _safe_file(job, name)
    rows = max(1, min(rows, 100))
    columns: list[str] = []
    data: list[list[str]] = []
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.reader(fh)
        columns = next(reader, [])
        for i, row in enumerate(reader):
            if i >= rows:
                break
            data.append(row)
    return {
        "columns": columns,
        "rows": data,
        "total_columns": len(columns),
        "returned_rows": len(data),
    }


@app.get("/jobs/{job_id}/files/{name}")
async def download_file(job_id: str, name: str, cols: str | None = None):
    """Download a file. With `cols` (a comma-separated column list) the CSV is
    streamed projected to just those columns, in the order requested — so an
    engineer can pull a handful of channels instead of the whole multi-GB file.
    Without `cols`, the original file is served as-is."""
    job = _require_job(job_id)
    path = _safe_file(job, name)
    if not cols:
        return FileResponse(path, filename=name, media_type="text/csv")

    requested = [c.strip() for c in cols.split(",") if c.strip()]
    with open(path, newline="", encoding="utf-8") as fh:
        header = next(csv.reader(fh), [])
    index_of = {c: i for i, c in enumerate(header)}
    indices = [index_of[c] for c in requested if c in index_of]
    if not indices:
        raise HTTPException(400, "none of the requested columns exist in this file")

    subset_name = (name[:-4] if name.endswith(".csv") else name) + ".subset.csv"

    def gen():
        # Accumulate rows and flush in ~64KB chunks: yielding per row forces a
        # threadpool context switch through Starlette for every line, which is
        # crippling on million-row files.
        with open(path, newline="", encoding="utf-8") as fh:
            reader = csv.reader(fh)
            buf = io.StringIO()
            writer = csv.writer(buf)
            for row in reader:
                writer.writerow([row[i] if i < len(row) else "" for i in indices])
                if buf.tell() >= 65536:
                    yield buf.getvalue()
                    buf.seek(0)
                    buf.truncate(0)
            if buf.tell():
                yield buf.getvalue()

    return StreamingResponse(
        gen(),
        media_type="text/csv",
        headers={"Content-Disposition": f'attachment; filename="{subset_name}"'},
    )


@app.get("/jobs/{job_id}/archive")
async def download_archive(job_id: str):
    """Stream a ZIP of all completed files (ZIP64, store-only — CSVs are huge)."""
    job = _require_job(job_id)
    done = [f for f in job.files if f.done]
    if not done:
        raise HTTPException(409, "no completed files to archive yet")
    mgr = _mgr()

    try:
        from zipstream import ZipStream  # zipstream-ng
    except Exception:
        raise HTTPException(501, "archive download requires zipstream-ng")

    zs = ZipStream(sized=False)
    for f in done:
        zs.add_path(mgr.file_path(f.name), f.name)

    return StreamingResponse(
        zs,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="logs_{job_id}.zip"'},
    )


@app.get("/events")
async def events(request: Request):
    mgr = _mgr()
    q = mgr.subscribe()
    # Prime with the current snapshot so a fresh client renders immediately.
    initial = [j.to_dict() for j in mgr.list()]

    async def gen():
        try:
            yield f"event: snapshot\ndata: {json.dumps(initial)}\n\n"
            while True:
                if await request.is_disconnected():
                    break
                try:
                    job = await asyncio.wait_for(q.get(), timeout=15)
                    yield f"data: {json.dumps(job)}\n\n"
                except asyncio.TimeoutError:
                    yield ": ping\n\n"
        finally:
            mgr.unsubscribe(q)

    return StreamingResponse(
        gen(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-store, no-transform", "X-Accel-Buffering": "no"},
    )
