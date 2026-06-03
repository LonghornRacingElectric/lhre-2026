FROM python:3.11.4 AS builder
COPY ./stack/processors/car_status/requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /app

# Processor code (main.py + pure classifier + db writer).
COPY ./stack/processors/car_status/ /app/stack/processors/car_status/

# Protobuf schemas for direct decode of sensor_data.
COPY ./stack/ingest/protobuf/ /app/stack/ingest/protobuf/

# DB layer for segment persistence (ORM models + QueryBuilder + sessions).
COPY ./analysis/ /app/analysis/
COPY ./net_configs.json /app/net_configs.json

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/car_status

CMD ["python", "./main.py"]
