FROM python:3.11.4 AS builder
COPY requirements.txt .
RUN pip install --upgrade pip
RUN pip install -r requirements.txt

RUN apt-get update
RUN apt-get install -y iproute2
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

FROM builder AS executor
WORKDIR /lap_timer
COPY . /lap_timer

CMD ["python", "./main.py"]