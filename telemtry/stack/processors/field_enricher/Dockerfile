FROM python:3.11.4 AS builder
COPY ./stack/processors/field_enricher/requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /app

COPY ./stack/processors/field_enricher/ /app/stack/processors/field_enricher/

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/field_enricher

CMD ["python", "./main.py"]
