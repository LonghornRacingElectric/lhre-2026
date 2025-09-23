FROM python:3.11.4 AS builder
COPY stack/ingest/requirements.txt .
RUN pip install --upgrade pip
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /app
COPY . /app

CMD ["python", "./stack/ingest/mqtt_handler.py"]