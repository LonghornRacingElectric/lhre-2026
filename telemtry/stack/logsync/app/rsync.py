"""rsync transfer engine.

One rsync invocation pulls every still-needed file for a job. Resumability and
bandwidth control come straight from rsync flags:

* ``--partial --append-verify`` — a killed transfer leaves the partial dest in
  place; the next run verifies the overlapping prefix and appends only the
  remainder. This is what makes pause/resume (and surviving restarts) free, and
  it also correctly extends the still-growing active log on a re-sync.
* ``--bwlimit`` — optional KB/s cap.
* ``--info=progress2`` — whole-transfer progress on one line we can parse.

Pausing == killing the process; resuming == running it again. The set of files
never changes within a job, so re-running is idempotent.
"""
from __future__ import annotations

import asyncio
import re
import shlex
from dataclasses import dataclass
from typing import Callable, Optional

from .config import config

# e.g. "  1,234,567  12%    1.23MB/s    0:00:10"
_PROGRESS_RE = re.compile(
    r"([\d,]+)\s+(\d+)%\s+([\d.]+)([kKmMgG]?)B/s"
)
_UNIT = {"": 1, "k": 1024, "m": 1024 ** 2, "g": 1024 ** 3}


@dataclass
class Progress:
    transferred_bytes: int
    percent: float
    rate_bps: float


def _parse_progress(chunk: str) -> Optional[Progress]:
    m = _PROGRESS_RE.search(chunk)
    if not m:
        return None
    transferred = int(m.group(1).replace(",", ""))
    percent = float(m.group(2))
    rate = float(m.group(3)) * _UNIT[m.group(4).lower()]
    return Progress(transferred_bytes=transferred, percent=percent, rate_bps=rate)


def build_cmd(listfile: str, dest: str, bwlimit_kbps: int) -> list[str]:
    transport = f"ssh -i {config.ssh_key} {config.ssh_opts}"
    cmd = [
        "rsync",
        "-e", transport,
        "--partial",
        "--append-verify",
        "--info=progress2",
        "--no-inc-recursive",
        "--timeout=120",
        f"--files-from={listfile}",
    ]
    if bwlimit_kbps and bwlimit_kbps > 0:
        cmd.append(f"--bwlimit={bwlimit_kbps}")
    # Source root + dest. files-from entries are bare filenames relative to root.
    src_root = f"{config.ssh_target}:{config.remote_log_dir}/"
    cmd += [src_root, dest if dest.endswith("/") else dest + "/"]
    return cmd


class RsyncRun:
    """A single rsync process whose progress is streamed to a callback."""

    def __init__(self, argv: list[str], on_progress: Callable[[Progress], None]):
        self._argv = argv
        self._on_progress = on_progress
        self._proc: Optional[asyncio.subprocess.Process] = None
        self._stderr_tail: list[str] = []

    async def start(self) -> None:
        self._proc = await asyncio.create_subprocess_exec(
            *self._argv,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )

    async def _pump_stdout(self) -> None:
        assert self._proc and self._proc.stdout
        buf = ""
        while True:
            data = await self._proc.stdout.read(4096)
            if not data:
                break
            # progress lines are \r-delimited; final summary is \n-delimited.
            buf += data.decode(errors="replace").replace("\r", "\n")
            *lines, buf = buf.split("\n")
            for line in lines:
                p = _parse_progress(line)
                if p:
                    self._on_progress(p)

    async def _pump_stderr(self) -> None:
        assert self._proc and self._proc.stderr
        while True:
            line = await self._proc.stderr.readline()
            if not line:
                break
            text = line.decode(errors="replace").rstrip()
            if text:
                self._stderr_tail.append(text)
                # bound memory
                if len(self._stderr_tail) > 50:
                    self._stderr_tail.pop(0)

    async def wait(self) -> int:
        """Run to completion, pumping stdout/stderr. Returns the exit code."""
        assert self._proc
        await asyncio.gather(self._pump_stdout(), self._pump_stderr())
        return await self._proc.wait()

    @property
    def stderr_tail(self) -> str:
        return "\n".join(self._stderr_tail[-10:])

    def terminate_sync(self) -> None:
        """Best-effort SIGTERM without awaiting — safe to call from a finally
        during task cancellation, where awaiting may be unreliable. Prevents
        orphaned rsync processes on shutdown."""
        if self._proc is None or self._proc.returncode is not None:
            return
        try:
            self._proc.terminate()
        except ProcessLookupError:
            pass

    async def terminate(self) -> None:
        """Stop the transfer. --partial keeps progress for the next run."""
        if self._proc is None or self._proc.returncode is not None:
            return
        try:
            self._proc.terminate()
            try:
                await asyncio.wait_for(self._proc.wait(), timeout=10)
            except asyncio.TimeoutError:
                self._proc.kill()
                await self._proc.wait()
        except ProcessLookupError:
            pass


# rsync exit codes that are safe to treat as "succeeded for our purposes".
# 0 = ok; 24 = some source files vanished (a rotated/closed log) which is benign.
RSYNC_OK_CODES = {0, 24}
