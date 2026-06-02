"""Environment-driven configuration for the logsync worker.

Every operationally-interesting knob is an env var so the service can be tuned
per deployment without code changes. Defaults target the BEVO Orion car as it
exists today (Pi reachable over Tailscale, logs under the lhre home dir).
"""
from __future__ import annotations

import os
from dataclasses import dataclass


def _int(name: str, default: int) -> int:
    raw = os.environ.get(name)
    if raw is None or raw.strip() == "":
        return default
    return int(raw)


def _float(name: str, default: float) -> float:
    raw = os.environ.get(name)
    if raw is None or raw.strip() == "":
        return default
    return float(raw)


def _str(name: str, default: str) -> str:
    raw = os.environ.get(name)
    if raw is None or raw.strip() == "":
        return default
    return raw


@dataclass(frozen=True)
class Config:
    # --- HTTP server ---
    host: str = _str("LOGSYNC_HOST", "0.0.0.0")
    port: int = _int("LOGSYNC_PORT", 8090)

    # --- Pi access (rsync over SSH over Tailscale) ---
    # user@host of the car Pi. Tailscale IP keeps it reachable on cellular.
    ssh_target: str = _str("BEVO_SSH_TARGET", "lhre@100.64.195.68")
    ssh_key: str = _str("BEVO_SSH_KEY", "/keys/id_logsync")
    # Extra ssh options appended to the rsync -e transport string.
    ssh_opts: str = _str(
        "BEVO_SSH_OPTS",
        "-o BatchMode=yes -o StrictHostKeyChecking=accept-new "
        "-o ServerAliveInterval=15 -o ServerAliveCountMax=4 -o ConnectTimeout=15",
    )
    # Remote logs dir, relative to the ssh user's home (or absolute).
    remote_log_dir: str = _str(
        "BEVO_LOG_DIR", "Documents/lhre/lhre-2026/BEVO/loggerd/logs"
    )
    log_prefix: str = _str("BEVO_LOG_PREFIX", "orion_")
    log_suffix: str = _str("BEVO_LOG_SUFFIX", ".csv")

    # --- Local staging / state ---
    staging_dir: str = _str("LOGSYNC_STAGING_DIR", "/data/staging")
    state_db: str = _str("LOGSYNC_STATE_DB", "/data/state/logsync.db")

    # --- Bandwidth / transfer ---
    # 0 == unlimited. Policy is pause-on-motion, but a cap can still be set.
    default_bwlimit_kbps: int = _int("LOGSYNC_DEFAULT_BWLIMIT_KBPS", 0)
    rsync_max_retries: int = _int("LOGSYNC_RSYNC_MAX_RETRIES", 5)
    rsync_retry_backoff_s: float = _float("LOGSYNC_RSYNC_RETRY_BACKOFF_S", 5.0)

    # --- Motion detection (Postgres orion DB) ---
    pg_dsn: str = _str("LOGSYNC_PG_DSN", "")  # if set, overrides the parts below
    pg_host: str = _str("LOGSYNC_PG_HOST", "127.0.0.1")
    pg_port: int = _int("LOGSYNC_PG_PORT", 5432)
    pg_db: str = _str("LOGSYNC_PG_DB", "orion")
    pg_user: str = _str("LOGSYNC_PG_USER", "electric")
    # Fall back to the same secret the viewer/ingest use.
    pg_password: str = _str("LOGSYNC_PG_PASSWORD", os.environ.get("ELECTRIC_PWD", ""))

    # Car is "moving" if the freshest sample is newer than staleness_ms AND its
    # speed exceeds speed_threshold. Stale/no telemetry => treated as not moving
    # (there is no live stream to protect, so transferring is safe).
    speed_threshold_mps: float = _float("LOGSYNC_SPEED_THRESHOLD_MPS", 0.5)
    motion_staleness_ms: int = _int("LOGSYNC_MOTION_STALENESS_MS", 10_000)
    motion_poll_interval_s: float = _float("LOGSYNC_MOTION_POLL_INTERVAL_S", 3.0)
    # Must read stationary continuously for this long before auto-resuming.
    motion_resume_stable_s: float = _float("LOGSYNC_MOTION_RESUME_STABLE_S", 10.0)

    @property
    def resolved_pg_dsn(self) -> str:
        if self.pg_dsn:
            return self.pg_dsn
        return (
            f"postgresql://{self.pg_user}:{self.pg_password}"
            f"@{self.pg_host}:{self.pg_port}/{self.pg_db}"
        )


config = Config()
