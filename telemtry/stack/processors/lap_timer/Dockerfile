FROM python:3.11.4

# Install python dependencies
COPY ./stack/processors/lap_timer/requirements.txt .
RUN pip install --upgrade pip
RUN pip install -r requirements.txt

# Install os dependencies and setup entrypoint
RUN apt-get update && apt-get install -y iproute2
COPY ./stack/processors/lap_timer/entrypoint.sh /entrypoint.sh
RUN sed -i 's/\r$//' /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

# Application setup
WORKDIR /app

# Copy the processor-specific code
COPY ./stack/processors/lap_timer/ /app/stack/processors/lap_timer/

# Copy dependencies
COPY ./stack/ingest/mqtt_handler.py /app/stack/ingest/mqtt_handler.py
COPY ./stack/ingest/protobuf/ /app/stack/ingest/protobuf/
COPY ./analysis/sql_utils/ /app/analysis/sql_utils/
COPY ./net_configs.json /app/net_configs.json

ENV PYTHONPATH /app
WORKDIR /app/stack/processors/lap_timer

CMD ["python", "./main.py"]