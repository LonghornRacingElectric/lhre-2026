FROM python:3.11.4 AS builder
COPY ./stack/processors/kafka_base/requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /app

# Copy the processor-specific code
COPY ./stack/processors/kafka_base/ /app/stack/processors/kafka_base/

# Copy dependencies
COPY ./stack/ingest/mqtt_handler.py /app/stack/ingest/mqtt_handler.py
COPY ./stack/ingest/protobuf/ /app/stack/ingest/protobuf/
COPY ./analysis/sql_utils/ /app/analysis/sql_utils/
COPY ./net_configs.json /app/net_configs.json

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/kafka_base

CMD ["python", "./main.py"]