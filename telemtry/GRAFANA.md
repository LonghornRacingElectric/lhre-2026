# Adding Grafana Dashboards
lhrelectric.org/grafana/ (dm any of the Telemetry members for password access)

## Realtime Dashboards
1. Create a regular new dashboard
2. Select "Real Time Data" as Data Source
3. Set the Topic to "grafana_data"
4. If all the fields appear on the dashboard:
- Time Series / If there are no default options to select only one value:
    1. Add an Override in the dashboard settings.
    2. Select "Fields with name matching regex".
    3. Type `^(?!name_of_property$).+` (i.e. `^(?!gps_heading$).+`). This essentially selects all the fields but the one you are looking for.
    4. Add an Override Property
    5. "Series > Hide in Area". This enables us to hide all the datapoints that we don't need.

## Retrospective Dashboards
1. Create a regular new dashboard.
2. Select the adequate datasource (Angelique, Telemetry...).
3. Write the corresponding SQL query (recommend asking an LLM to generate it based on your needs).