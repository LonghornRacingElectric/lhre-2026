FROM python:3.11-slim

# rsync + ssh client are the transfer engine; tini for clean signal handling so
# pause/cancel (SIGTERM to rsync children) works promptly.
RUN apt-get update \
    && apt-get install -y --no-install-recommends rsync openssh-client tini ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

# Data dirs (overridden by volume mounts in compose).
RUN mkdir -p /data/staging /data/state

EXPOSE 8090
ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["sh", "-c", "uvicorn app.main:app --host ${LOGSYNC_HOST:-0.0.0.0} --port ${LOGSYNC_PORT:-8090}"]
