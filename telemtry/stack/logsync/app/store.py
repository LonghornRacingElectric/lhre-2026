"""SQLite persistence for jobs so they survive worker restarts.

The whole Job (including its file list and progress) is serialized as JSON in a
single row. Volume is tiny (one row per job) and the access pattern is simple,
so a document-style blob beats a normalized schema here.
"""
from __future__ import annotations

import json
import os
import sqlite3
import threading
from typing import Optional

from .models import QUALITY_VALUES, Annotation, FileProgress, Job, JobState


class Store:
    def __init__(self, path: str):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self._conn = sqlite3.connect(path, check_same_thread=False)
        self._conn.execute(
            """
            CREATE TABLE IF NOT EXISTS jobs (
                id          TEXT PRIMARY KEY,
                created_ms  INTEGER NOT NULL,
                updated_ms  INTEGER NOT NULL,
                state       TEXT NOT NULL,
                data        TEXT NOT NULL
            )
            """
        )
        self._conn.commit()

    @staticmethod
    def _serialize(job: Job) -> str:
        return json.dumps(job.to_dict())

    @staticmethod
    def _deserialize(data: str) -> Job:
        d = json.loads(data)
        files = [FileProgress(**f) for f in d.get("files", [])]
        return Job(
            id=d["id"],
            from_ms=d["from_ms"],
            to_ms=d["to_ms"],
            bwlimit_kbps=d.get("bwlimit_kbps", 0),
            state=JobState(d["state"]),
            created_ms=d.get("created_ms", 0),
            updated_ms=d.get("updated_ms", 0),
            files=files,
            total_bytes=d.get("total_bytes", 0),
            transferred_bytes=d.get("transferred_bytes", 0),
            rate_bps=d.get("rate_bps", 0.0),
            attempts=d.get("attempts", 0),
            error=d.get("error"),
            last_motion=d.get("last_motion"),
        )

    def upsert(self, job: Job) -> None:
        self._conn.execute(
            "INSERT INTO jobs (id, created_ms, updated_ms, state, data) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET updated_ms=excluded.updated_ms, "
            "state=excluded.state, data=excluded.data",
            (job.id, job.created_ms, job.updated_ms, job.state.value, self._serialize(job)),
        )
        self._conn.commit()

    def get(self, job_id: str) -> Optional[Job]:
        row = self._conn.execute("SELECT data FROM jobs WHERE id=?", (job_id,)).fetchone()
        return self._deserialize(row[0]) if row else None

    def list(self) -> list[Job]:
        rows = self._conn.execute(
            "SELECT data FROM jobs ORDER BY created_ms DESC"
        ).fetchall()
        return [self._deserialize(r[0]) for r in rows]


class AnnotationStore:
    """Per-file trackside annotations, keyed by loggerd filename.

    Shares the same SQLite file as the job Store (its own table) — the data is
    tiny and lives next to the jobs it describes. Like jobs, each annotation is
    a JSON blob in one row; the only column we query on is the file name.
    """

    def __init__(self, path: str):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self._conn = sqlite3.connect(path, check_same_thread=False)
        # The annotation endpoints are async (so calls are already serialized on
        # the event loop), but guard the connection anyway in case a future
        # caller hits it from Starlette's threadpool — concurrent use of one
        # sqlite connection is unsafe.
        self._lock = threading.Lock()
        with self._lock:
            self._conn.execute(
                """
                CREATE TABLE IF NOT EXISTS annotations (
                    name        TEXT PRIMARY KEY,
                    updated_ms  INTEGER NOT NULL,
                    data        TEXT NOT NULL
                )
                """
            )
            self._conn.commit()

    @staticmethod
    def _deserialize(data: str) -> Annotation:
        d = json.loads(data)
        quality = d.get("quality", "")
        if quality not in QUALITY_VALUES:
            quality = ""
        return Annotation(
            name=d["name"],
            notes=d.get("notes", ""),
            tags=list(d.get("tags", [])),
            driver=d.get("driver", ""),
            track=d.get("track", ""),
            session=d.get("session", ""),
            weather=d.get("weather", ""),
            tires=d.get("tires", ""),
            setup=d.get("setup", ""),
            starred=bool(d.get("starred", False)),
            quality=quality,
            updated_ms=d.get("updated_ms", 0),
        )

    def get(self, name: str) -> Optional[Annotation]:
        with self._lock:
            row = self._conn.execute(
                "SELECT data FROM annotations WHERE name=?", (name,)
            ).fetchone()
        return self._deserialize(row[0]) if row else None

    def list(self) -> list[Annotation]:
        with self._lock:
            rows = self._conn.execute("SELECT data FROM annotations").fetchall()
        return [self._deserialize(r[0]) for r in rows]

    def upsert(self, ann: Annotation) -> None:
        with self._lock:
            self._conn.execute(
                "INSERT INTO annotations (name, updated_ms, data) VALUES (?, ?, ?) "
                "ON CONFLICT(name) DO UPDATE SET updated_ms=excluded.updated_ms, "
                "data=excluded.data",
                (ann.name, ann.updated_ms, json.dumps(ann.to_dict())),
            )
            self._conn.commit()

    def delete(self, name: str) -> None:
        with self._lock:
            self._conn.execute("DELETE FROM annotations WHERE name=?", (name,))
            self._conn.commit()
