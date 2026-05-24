FROM python:3.11.4 AS builder
COPY ./stack/processors/kafka_base/requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /app

# Copy the processor-specific code
COPY ./stack/processors/kafka_base/ /app/stack/processors/kafka_base/

# Copy protobuf schemas for direct decode
COPY ./stack/ingest/protobuf/ /app/stack/ingest/protobuf/

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/kafka_base

CMD ["python", "./main.py"]
