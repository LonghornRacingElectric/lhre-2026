"""Vehicle-motion detection from the Orion telemetry DB.

The freshest ``packet``/``dynamics`` row tells us whether the car is moving
right now. Speed is the max magnitude across the four wheel-speed channels and
GPS speed (any one being high means motion). A reading only counts as "moving"
if it is also recent: if telemetry is stale or absent there is no live stream
competing for cellular bandwidth, so transferring is safe.
"""
from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass

import asyncpg

from .config import config

# Latest sample joined across packet (time) and dynamics (speeds).
_QUERY = """
    SELECT p."time" AS time_ms,
           d.wheel_speed,
           d.gps_speed,
           d.bl_wheel_speed,
           d.br_wheel_speed,
           d.fl_wheel_speed,
           d.fr_wheel_speed
    FROM dynamics d
    JOIN packet p ON p.packet_id = d.packet_id
    ORDER BY d.packet_id DESC
    LIMIT 1
"""


@dataclass
class Motion:
    moving: bool
    speed_mps: float | None     # best-known speed, None if no data
    sample_age_ms: int | None   # how old the freshest sample is
    recent: bool                # sample is fresher than the staleness window
    error: str | None = None

    def to_dict(self) -> dict:
        return {
            "moving": self.moving,
            "speed_mps": self.speed_mps,
            "sample_age_ms": self.sample_age_ms,
            "recent": self.recent,
            "error": self.error,
        }


class MotionMonitor:
    def __init__(self) -> None:
        self._pool: asyncpg.Pool | None = None
        self._lock = asyncio.Lock()

    async def _get_pool(self) -> asyncpg.Pool:
        # Lock so concurrent callers (UI /motion poll + worker loop) don't each
        # build a pool across the create_pool await and leak one.
        async with self._lock:
            if self._pool is None:
                self._pool = await asyncpg.create_pool(
                    dsn=config.resolved_pg_dsn, min_size=1, max_size=2, command_timeout=5
                )
            return self._pool

    async def close(self) -> None:
        if self._pool is not None:
            await self._pool.close()
            self._pool = None

    async def read(self) -> Motion:
        try:
            pool = await self._get_pool()
            row = await pool.fetchrow(_QUERY)
        except Exception as e:  # DB down, schema mismatch, etc.
            # Fail open: we can't prove the car is moving, so don't block forever.
            return Motion(moving=False, speed_mps=None, sample_age_ms=None,
                          recent=False, error=str(e))

        if row is None or row["time_ms"] is None:
            return Motion(moving=False, speed_mps=None, sample_age_ms=None, recent=False)

        speeds = [
            row["wheel_speed"], row["gps_speed"],
            row["bl_wheel_speed"], row["br_wheel_speed"],
            row["fl_wheel_speed"], row["fr_wheel_speed"],
        ]
        speed = max((abs(s) for s in speeds if s is not None), default=None)

        now_ms = int(time.time() * 1000)
        age_ms = now_ms - int(row["time_ms"])
        recent = age_ms <= config.motion_staleness_ms
        moving = recent and speed is not None and speed > config.speed_threshold_mps
        return Motion(moving=moving, speed_mps=speed, sample_age_ms=age_ms, recent=recent)
