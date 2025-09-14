FROM python:3.11.4 AS builder
COPY requirements.txt .
RUN pip install -r requirements.txt

FROM builder AS executor
WORKDIR /gps_classifier
COPY . /gps_classifier

CMD ["python", "./main.py"]