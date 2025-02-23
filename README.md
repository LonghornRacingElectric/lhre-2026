### To start the pipeline, create two terminals. In the first, `cd` to `/stack/` and run `docker compose up`. In the second terminal, activate a venv, `cd` to `/analysis/database/viewer_tool` and run `python index.py`. To ingest data, visit `host:5000` and start an event--data will only be ingested after the start time button has been pressed.

### Notes on jank solutions:
#### 1. Automatic SQL event index injection
When rebuilding SQL script, add following function to `db_init.sql` output after `SET check_function_bodies = false;`: 
```sql
CREATE OR REPLACE FUNCTION public.get_event_index (car smallint, day smallint)
	RETURNS smallint
	LANGUAGE plpgsql
	IMMUTABLE
	STRICT
	AS
$$
DECLARE ind smallint;
BEGIN
	SELECT COUNT(event_id)
	INTO ind
	FROM event
	WHERE car_id = car AND day_id = day;
	RETURN ind + 1;
END;
$$;
ALTER FUNCTION public.get_event_index(smallint,smallint) OWNER TO electric;
```

#### 2. PostgreSQL Temporary Filesystem
Description: The Postgres DB image requires that a temporary filesystem be provided, necessitating special docker-compose configurations as follows:
```yaml
  db:
    image: postgres
    container_name: db
    restart: unless-stopped
    tmpfs:
      - /tmp
      - /run/postgresql
```


#### 3. Python Image Awareness
Description: Docker creates its own network for containers in a docker compose. Thus, containers are dereferenced from localhost and must be addressed by their container names. For development purposes where scripts are run outside of a Docker container, `localhost` should be used. To address this discrepency, a `IN_DOCKER` environment variable is added to `/stack/docker-compose.yaml` allowing Python to programmatically determine whether they are being executed in Docker.

Additionally, the `PYTHONUNBUFFERED` environment variable is necessary to see output from Python in logs.
```yaml
    python_client:
    image: telemetry_backend
    container_name: python_client
    environment:
      PYTHONUNBUFFERED: 1   # Allows output in Docker Compose console
      IN_DOCKER: 1          # Custom env variable used for IP resolution
```

#### 4. Cheesed Imports
Description: To limit software overhead, the data pipeline only includes files in the `/stack/` folder. However, some utilities found in `/analysis/` are necessary for the pipeline to run. In an effort to avoid including too many requirements to the pipeline itself, these utilities must be included using a bind mount and filesystem manipulation.
At first glance, code as follows (found in `/stack/ingest/mqtt_handler.py`) makes no sense:
```python
if os.getenv('IN_DOCKER'):
    from db_handler import DBHandler, get_table_column_specs    # Cheesed import statement using bind mount
else:
    from analysis.sql_utils.db_handler import DBHandler, get_table_column_specs
```
Using the `IN_DOCKER` environment variable we can tell if the program is being executed on the main filesystem where a normal import such as the latter will work. If run in Docker, individual files with important utilities should be bind mounted as follows, using the first import method:
```yaml
  python_client:
    image: telemetry_backend
    container_name: python_client
    volumes:
      - ../analysis/sql_utils/db_handler.py:/ingest/db_handler.py
```

