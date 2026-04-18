FROM ubuntu:24.04 AS executor

ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    VENV_PATH=/opt/venv \
    PATH="/opt/venv/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-venv \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv "${VENV_PATH}"
COPY ./stack/processors/steer_sus/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

WORKDIR /app

# Copy the processor-specific code
COPY ./stack/processors/steer_sus/ /app/stack/processors/steer_sus/

# Copy protobuf schemas for direct decode
COPY ./stack/ingest/protobuf/ /app/stack/ingest/protobuf/

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/steer_sus

CMD ["python", "./main.py"]
