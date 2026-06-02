"""Data models shared across the worker."""
from __future__ import annotations

from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import Optional


class JobState(str, Enum):
    QUEUED = "queued"            # accepted, waiting for a worker slot
    RUNNING = "running"          # rsync actively transferring
    PAUSED_MOTION = "paused_motion"  # auto-paused: car is moving
    PAUSED = "paused"            # manually paused by an operator
    COMPLETED = "completed"      # all selected files transferred
    FAILED = "failed"            # gave up after retries / fatal error
    CANCELED = "canceled"        # operator canceled

    @property
    def is_terminal(self) -> bool:
        return self in (JobState.COMPLETED, JobState.FAILED, JobState.CANCELED)

    @property
    def is_active(self) -> bool:
        """States that should be resumed on startup."""
        return self in (
            JobState.QUEUED,
            JobState.RUNNING,
            JobState.PAUSED_MOTION,
            JobState.PAUSED,
        )


@dataclass
class RemoteFile:
    """A log file on the Pi, with the time window it covers."""
    name: str
    start_ms: int            # session start, parsed from the filename
    end_ms: int              # last write, from mtime
    size: int                # bytes, at listing time

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class FileProgress:
    name: str
    size: int                # expected bytes (from the remote listing)
    transferred: int = 0     # bytes present locally
    done: bool = False

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class Job:
    id: str
    from_ms: int
    to_ms: int
    bwlimit_kbps: int
    state: JobState = JobState.QUEUED
    created_ms: int = 0
    updated_ms: int = 0
    # Files selected for this job (immutable once created).
    files: list[FileProgress] = field(default_factory=list)
    total_bytes: int = 0
    transferred_bytes: int = 0
    # Most recent transfer rate reported by rsync, bytes/sec.
    rate_bps: float = 0.0
    attempts: int = 0
    error: Optional[str] = None
    # Last motion reading that affected this job, for UI display.
    last_motion: Optional[dict] = None

    def to_dict(self) -> dict:
        d = asdict(self)
        d["state"] = self.state.value
        pct = 0.0
        if self.total_bytes > 0:
            pct = min(100.0, 100.0 * self.transferred_bytes / self.total_bytes)
        d["percent"] = round(pct, 2)
        d["file_count"] = len(self.files)
        d["files_done"] = sum(1 for f in self.files if f.done)
        return d
