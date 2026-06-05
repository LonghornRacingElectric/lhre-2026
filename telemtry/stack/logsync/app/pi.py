"""Listing and time-range selection of Pi log files over SSH.

Loggerd writes one CSV per session named ``orion_<startMs>.csv`` and keeps
appending to it for the duration of that session. So a file covers the window
``[startMs, mtime]``: the filename gives the start, the mtime gives the end.
A file belongs to a requested ``[from, to]`` window iff those intervals overlap.
"""
from __future__ import annotations

import asyncio
import shlex

from .config import config
from .models import RemoteFile


def _ssh_base_cmd() -> list[str]:
    # Reuse the same key/opts the rsync transport uses, so a successful listing
    # guarantees rsync can authenticate too.
    return ["ssh", "-i", config.ssh_key, *shlex.split(config.ssh_opts), config.ssh_target]


def _parse_start_ms(name: str) -> int | None:
    if not (name.startswith(config.log_prefix) and name.endswith(config.log_suffix)):
        return None
    core = name[len(config.log_prefix): -len(config.log_suffix)]
    if not core.isdigit():
        return None
    return int(core)


async def list_remote_logs() -> list[RemoteFile]:
    """SSH to the Pi and enumerate log files with size + mtime.

    Uses ``find -printf`` (epoch mtime, raw bytes, bare filename) which is
    locale-independent and trivially parseable, unlike ``ls``.
    """
    remote = shlex.quote(config.remote_log_dir)
    pattern = shlex.quote(f"{config.log_prefix}*{config.log_suffix}")
    # %f = filename, %s = size bytes, %T@ = mtime epoch seconds (float)
    find_cmd = (
        f"find {remote} -maxdepth 1 -type f -name {pattern} "
        f"-printf '%f\\t%s\\t%T@\\n'"
    )
    cmd = [*_ssh_base_cmd(), find_cmd]

    proc = await asyncio.create_subprocess_exec(
        *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
    )
    # Bound the call so a hung cellular link can't wedge the request indefinitely
    # (SSH keepalives would eventually bail too, but this is the hard ceiling).
    try:
        out, err = await asyncio.wait_for(proc.communicate(), timeout=45.0)
    except asyncio.TimeoutError:
        try:
            proc.kill()
        except ProcessLookupError:
            pass
        await proc.communicate()
        raise RuntimeError("timed out listing remote logs over SSH")
    if proc.returncode != 0:
        raise RuntimeError(
            f"Failed to list remote logs (exit {proc.returncode}): "
            f"{err.decode(errors='replace').strip()}"
        )

    files: list[RemoteFile] = []
    for line in out.decode(errors="replace").splitlines():
        parts = line.split("\t")
        if len(parts) != 3:
            continue
        name, size_s, mtime_s = parts
        start_ms = _parse_start_ms(name)
        if start_ms is None:
            continue
        try:
            size = int(size_s)
            end_ms = int(float(mtime_s) * 1000)
        except ValueError:
            continue
        # A session can't end before it starts; guard against odd mtimes.
        files.append(RemoteFile(name=name, start_ms=start_ms, end_ms=max(end_ms, start_ms), size=size))

    files.sort(key=lambda f: f.start_ms)
    return files


def select_in_range(files: list[RemoteFile], from_ms: int, to_ms: int) -> list[RemoteFile]:
    """Return files whose [start, end] overlaps the requested [from, to]."""
    if to_ms < from_ms:
        from_ms, to_ms = to_ms, from_ms
    return [f for f in files if f.start_ms <= to_ms and f.end_ms >= from_ms]
