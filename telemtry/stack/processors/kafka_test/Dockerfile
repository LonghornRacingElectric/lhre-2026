FROM python:3.11.4 AS builder
COPY requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /kafka_consumer
COPY . /kafka_consumer

CMD ["python", "-u", "./main.py"]