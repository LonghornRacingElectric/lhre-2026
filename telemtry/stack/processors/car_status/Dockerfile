FROM python:3.11.4 AS builder
COPY ./stack/processors/car_status/requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /app

# Processor code (main.py + pure classifier).
COPY ./stack/processors/car_status/ /app/stack/processors/car_status/

# Protobuf schemas for direct decode of sensor_data.
COPY ./stack/ingest/protobuf/ /app/stack/ingest/protobuf/

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/car_status

CMD ["python", "./main.py"]
