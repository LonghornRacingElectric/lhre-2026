"""Job orchestration: the state machine that drives transfers.

Design notes
------------
* **One transfer at a time.** A single worker loop pulls job ids from a queue.
  Bounding to one rsync keeps cellular usage predictable, and since "car is
  moving" affects *every* transfer there is no benefit to running several.
* **Pause/resume == kill/respawn rsync.** ``--partial --append-verify`` makes
  re-running idempotent, so the same mechanism covers manual pause, motion
  pause, and surviving a worker restart.
* **Motion pause holds the slot; manual pause yields it.** If the car is moving
  no transfer should run, so a motion-paused job keeps the worker and waits.
  A manually paused job releases the worker so other jobs can proceed, and is
  re-enqueued on resume.
* **Progress is read from disk**, not from rsync's per-run byte counter (which
  resets each invocation). Summing local file sizes is accurate across any
  number of resumed runs.
"""
from __future__ import annotations

import asyncio
import os
import shutil
import time
from dataclasses import dataclass, field

from .config import config
from .models import FileProgress, Job, JobState, RemoteFile
from .motion import MotionMonitor
from .rsync import RSYNC_OK_CODES, Progress, RsyncRun, build_cmd
from .store import Store


def _now_ms() -> int:
    return int(time.time() * 1000)


@dataclass
class _Control:
    """Runtime-only control flags for a job (never persisted)."""
    cancel: asyncio.Event = field(default_factory=asyncio.Event)
    resume: asyncio.Event = field(default_factory=asyncio.Event)
    paused: bool = False


class JobManager:
    def __init__(self) -> None:
        self.store = Store(config.state_db)
        self.motion = MotionMonitor()
        self._jobs: dict[str, Job] = {}
        self._controls: dict[str, _Control] = {}
        self._queue: asyncio.Queue[str] = asyncio.Queue()
        self._subscribers: set[asyncio.Queue] = set()
        self._worker_task: asyncio.Task | None = None
        self._seq = 0

    # ----- lifecycle -----

    async def start(self) -> None:
        # Reload persisted jobs and resume anything that was mid-flight.
        for job in self.store.list():
            self._jobs[job.id] = job
            self._controls[job.id] = _Control()
            if job.state in (JobState.QUEUED, JobState.RUNNING, JobState.PAUSED_MOTION):
                # Treat an interrupted run as queued; it resumes from --partial.
                job.state = JobState.QUEUED
                self._persist(job)
                self._queue.put_nowait(job.id)
            elif job.state == JobState.PAUSED:
                self._controls[job.id].paused = True
        self._worker_task = asyncio.create_task(self._worker_loop())

    async def stop(self) -> None:
        if self._worker_task:
            self._worker_task.cancel()
            # Await so the worker's finally blocks (rsync cleanup) run to completion.
            try:
                await self._worker_task
            except asyncio.CancelledError:
                pass
        await self.motion.close()

    # ----- public API used by the HTTP layer -----

    async def create_job(self, from_ms: int, to_ms: int, files: list[RemoteFile],
                         bwlimit_kbps: int | None) -> Job:
        self._seq += 1
        job_id = f"{_now_ms()}-{self._seq}"
        job = Job(
            id=job_id,
            from_ms=from_ms,
            to_ms=to_ms,
            bwlimit_kbps=bwlimit_kbps if bwlimit_kbps is not None else config.default_bwlimit_kbps,
            state=JobState.QUEUED,
            created_ms=_now_ms(),
            updated_ms=_now_ms(),
            files=[FileProgress(name=f.name, size=f.size) for f in files],
            total_bytes=sum(f.size for f in files),
        )
        self._jobs[job_id] = job
        self._controls[job_id] = _Control()
        self._persist(job)
        self._queue.put_nowait(job_id)
        return job

    def get(self, job_id: str) -> Job | None:
        return self._jobs.get(job_id)

    def list(self) -> list[Job]:
        return sorted(self._jobs.values(), key=lambda j: j.created_ms, reverse=True)

    def pause(self, job_id: str) -> bool:
        job = self._jobs.get(job_id)
        ctrl = self._controls.get(job_id)
        if not job or not ctrl or job.state.is_terminal:
            return False
        ctrl.paused = True
        ctrl.resume.clear()
        return True

    def resume(self, job_id: str) -> bool:
        job = self._jobs.get(job_id)
        ctrl = self._controls.get(job_id)
        if not job or not ctrl:
            return False
        if job.state not in (JobState.PAUSED, JobState.PAUSED_MOTION, JobState.QUEUED):
            return False
        ctrl.paused = False
        ctrl.resume.set()
        # If it had yielded the worker (manual pause), put it back in line.
        if job.state == JobState.PAUSED:
            self._set_state(job, JobState.QUEUED)
            self._queue.put_nowait(job.id)
        return True

    def cancel(self, job_id: str) -> bool:
        job = self._jobs.get(job_id)
        ctrl = self._controls.get(job_id)
        if not job or not ctrl or job.state.is_terminal:
            return False
        ctrl.cancel.set()
        ctrl.resume.set()  # wake any waiter
        # If it wasn't actively running, mark it now.
        if job.state in (JobState.PAUSED, JobState.QUEUED):
            self._set_state(job, JobState.CANCELED)
        return True

    def staging_path(self, job_id: str) -> str:
        return os.path.join(config.staging_dir, job_id)

    # ----- SSE pub/sub -----

    def subscribe(self) -> asyncio.Queue:
        q: asyncio.Queue = asyncio.Queue(maxsize=100)
        self._subscribers.add(q)
        return q

    def unsubscribe(self, q: asyncio.Queue) -> None:
        self._subscribers.discard(q)

    def _publish(self, job: Job) -> None:
        snapshot = job.to_dict()
        for q in list(self._subscribers):
            try:
                q.put_nowait(snapshot)
            except asyncio.QueueFull:
                pass

    def _persist(self, job: Job) -> None:
        job.updated_ms = _now_ms()
        self.store.upsert(job)
        self._publish(job)

    def _set_state(self, job: Job, state: JobState) -> None:
        if job.state != state:
            job.state = state
        self._persist(job)

    # ----- worker -----

    async def _worker_loop(self) -> None:
        while True:
            job_id = await self._queue.get()
            job = self._jobs.get(job_id)
            ctrl = self._controls.get(job_id)
            if not job or not ctrl or job.state.is_terminal:
                continue
            if ctrl.cancel.is_set():
                self._set_state(job, JobState.CANCELED)
                continue
            try:
                await self._run_job(job, ctrl)
            except Exception as e:  # never let one job kill the worker
                job.error = f"worker error: {e}"
                self._set_state(job, JobState.FAILED)

    def _update_local_progress(self, job: Job) -> None:
        dest = self.staging_path(job.id)
        total = 0
        for f in job.files:
            path = os.path.join(dest, f.name)
            sz = os.path.getsize(path) if os.path.exists(path) else 0
            sz = min(sz, f.size)
            f.transferred = sz
            # A file is complete once we have all its bytes. Empty sessions
            # (0-byte files) are trivially complete — there is nothing to pull.
            f.done = sz >= f.size
            total += sz
        job.transferred_bytes = total

    def _all_done(self, job: Job) -> bool:
        return bool(job.files) and all(f.done for f in job.files)

    def _write_listfile(self, job: Job, dest: str) -> str:
        listfile = os.path.join(dest, ".files-from")
        with open(listfile, "w") as fh:
            for f in job.files:
                fh.write(f.name + "\n")
        return listfile

    async def _run_job(self, job: Job, ctrl: _Control) -> None:
        dest = self.staging_path(job.id)
        os.makedirs(dest, exist_ok=True)
        listfile = self._write_listfile(job, dest)

        self._update_local_progress(job)
        if self._all_done(job):
            self._set_state(job, JobState.COMPLETED)
            return

        last_progress_bytes = -1
        stalled_rounds = 0

        while True:
            if ctrl.cancel.is_set():
                self._set_state(job, JobState.CANCELED)
                return

            self._update_local_progress(job)
            if self._all_done(job):
                self._set_state(job, JobState.COMPLETED)
                return

            # --- gate: manual pause yields the worker slot ---
            if ctrl.paused:
                self._set_state(job, JobState.PAUSED)
                return

            # --- gate: disk space ---
            needed = job.total_bytes - job.transferred_bytes
            free = shutil.disk_usage(config.staging_dir).free
            if needed > free:
                job.error = (
                    f"insufficient disk space: need ~{needed} bytes, "
                    f"{free} free in staging"
                )
                self._set_state(job, JobState.FAILED)
                return

            # --- gate: motion holds the slot and waits ---
            motion = await self.motion.read()
            job.last_motion = motion.to_dict()
            if motion.moving:
                self._set_state(job, JobState.PAUSED_MOTION)
                await self._wait_motion_clear(job, ctrl)
                continue

            # --- transfer ---
            job.attempts += 1
            self._set_state(job, JobState.RUNNING)

            def on_progress(p: Progress) -> None:
                job.rate_bps = p.rate_bps

            run = RsyncRun(build_cmd(listfile, dest, job.bwlimit_kbps), on_progress)
            await run.start()
            rc = await self._supervise(job, ctrl, run)

            self._update_local_progress(job)

            if rc is None:
                # We interrupted it (cancel / pause / motion). Loop re-evaluates.
                continue

            if rc in RSYNC_OK_CODES:
                if self._all_done(job):
                    job.rate_bps = 0.0
                    self._set_state(job, JobState.COMPLETED)
                    return
                # rsync exited ok but not everything landed — guard against loops.
                if job.transferred_bytes <= last_progress_bytes:
                    stalled_rounds += 1
                    if stalled_rounds >= 2:
                        job.error = "rsync completed but files are still incomplete"
                        self._set_state(job, JobState.FAILED)
                        return
                else:
                    stalled_rounds = 0
                last_progress_bytes = job.transferred_bytes
                continue

            # rsync error — retry with backoff, then give up.
            if job.attempts >= config.rsync_max_retries:
                job.error = f"rsync failed (exit {rc}): {run.stderr_tail}"
                self._set_state(job, JobState.FAILED)
                return
            self._persist(job)
            await asyncio.sleep(config.rsync_retry_backoff_s)

    async def _supervise(self, job: Job, ctrl: _Control, run: RsyncRun) -> int | None:
        """Wait for rsync, preempting on cancel / manual pause / motion.

        Returns the exit code, or None if we terminated the process ourselves.
        """
        wait_task = asyncio.create_task(run.wait())
        poll = config.motion_poll_interval_s
        try:
            while True:
                done, _ = await asyncio.wait({wait_task}, timeout=poll)
                if wait_task in done:
                    return wait_task.result()

                self._update_local_progress(job)
                self._persist(job)

                if ctrl.cancel.is_set() or ctrl.paused:
                    await run.terminate()
                    await wait_task
                    return None

                motion = await self.motion.read()
                job.last_motion = motion.to_dict()
                if motion.moving:
                    await run.terminate()
                    await wait_task
                    return None
        finally:
            if not wait_task.done():
                wait_task.cancel()
            # If we're being cancelled (e.g. shutdown), don't leave rsync orphaned.
            run.terminate_sync()

    async def _wait_motion_clear(self, job: Job, ctrl: _Control) -> None:
        """Block while the car is moving; return once stationary and stable."""
        stable_since: float | None = None
        while True:
            if ctrl.cancel.is_set() or ctrl.paused:
                return
            motion = await self.motion.read()
            job.last_motion = motion.to_dict()
            self._persist(job)
            if motion.moving:
                stable_since = None
            else:
                now = time.monotonic()
                if stable_since is None:
                    stable_since = now
                elif now - stable_since >= config.motion_resume_stable_s:
                    return
            await asyncio.sleep(config.motion_poll_interval_s)
