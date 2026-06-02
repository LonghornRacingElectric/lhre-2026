"""HTTP surface for the logsync worker (FastAPI).

The viewer proxies the browser to these endpoints. Everything is JSON except
the SSE progress stream and the file/archive downloads.
"""
from __future__ import annotations

import asyncio
import json
import os
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, StreamingResponse
from pydantic import BaseModel

from .config import config
from .manager import JobManager
from .pi import list_remote_logs, select_in_range

manager: JobManager | None = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global manager
    os.makedirs(config.staging_dir, exist_ok=True)
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


class CreateJobBody(BaseModel):
    from_ms: int
    to_ms: int
    bwlimit_kbps: int | None = None


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


@app.get("/jobs/{job_id}/files/{name}")
async def download_file(job_id: str, name: str):
    job = _require_job(job_id)
    path = _safe_file(job, name)
    return FileResponse(path, filename=name, media_type="text/csv")


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
