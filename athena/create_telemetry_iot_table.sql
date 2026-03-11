DROP TABLE IF EXISTS iot_telemetry.telemetry_raw;

CREATE EXTERNAL TABLE iot_telemetry.telemetry_raw (
  timestamp bigint,
  room string,
  device_id string,
  sensors struct<
    temperature_c: string,
    humidity_pct: string,
    mq9_raw: int,
    mq9_ppm: string
  >,
  meta struct<
    firmware: string,
    rssi: int
  >
)
ROW FORMAT SERDE 'org.openx.data.jsonserde.JsonSerDe'
LOCATION 's3://enviro-node-bucket-01/sensors/device=sensors/';
