import json
import os
import time
from datetime import datetime, timezone
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

print("InfluxDB client imported successfully")


def is_timestamp_valid(ts: int, max_age_seconds: int = 300):
    """
    Returns True if the timestamp is within an acceptable window.
    Rejects:
      - timestamps before 2020
      - timestamps more than max_age_seconds in the past
      - timestamps more than 5 minutes in the future
    """
    if ts < 1577836800:  # Jan 1 2020
        return False

    now = int(time.time())

    if ts > now + 300:  # more than 5 minutes in the future
        return False

    if now - ts > max_age_seconds:  # too old
        return False

    return True


def enviro_node_handler(event, context):

    print("EVENT RECEIVED:", event)

    # --- Normalize event structure ---
    # If coming from AWS IoT, event is already a dict.
    # If coming from a manual test, event["payload"] is a JSON string.
    if "payload" in event and isinstance(event["payload"], str):
        try:
            data = json.loads(event["payload"])
        except json.JSONDecodeError:
            print("[ERROR] Payload is not valid JSON:", event["payload"])
            return {
                "statusCode": 400,
                "body": json.dumps({"message": "Invalid payload JSON"})
            }
    else:
        data = event


    # 3. Extract fields from your nested JSON

    device_id = data["device_id"]
    raw_timestamp = data["timestamp"]
    room = data["room"]

    sensors = data["sensors"]
    temp_c_raw = sensors["temperature_c"]
    humidity_raw = sensors["humidity_pct"]
    mq9_raw_raw = sensors["mq9_raw"]
    mq9_ppm_raw = sensors["mq9_ppm"]

    meta = data["meta"]
    firmware = meta["firmware"]
    rssi_raw = meta["rssi"]


    try:
        temp_c = float(temp_c_raw)
        humidity = float(humidity_raw)
        mq9_raw = int(mq9_raw_raw)
        mq9_ppm = float(mq9_ppm_raw)
        rssi = int(rssi_raw)
    except (ValueError, TypeError) as e:
        print("[ERROR] Invalid sensor values:", e)
        return {
            "statusCode": 400,
            "body": json.dumps({"message": "Invalid sensor outputs"})
        }
 


    # 4. Debug print
    print(f"Received telemetry from {device_id} at {raw_timestamp}:")
    print(json.dumps(data, indent=2))

    
    # 5. Data validation
    float_list = [temp_c, humidity, mq9_raw, mq9_ppm, rssi]

    try:
        for value in float_list:
            float(value)
    except (TypeError, ValueError):
        print("Validation failed. Incoming values:", float_list)
        return {
            "statusCode": 400,
            "body": json.dumps({"message": "Invalid sensor outputs"})
        }

    if not (0 <= humidity <= 100):
        print("Validation failed. Humidity out of range:", humidity)
        return {
            "statusCode": 400,
            "body": json.dumps({"message": "Humidity out of range"})
        }

    # --- Timestamp validation BEFORE converting to nanoseconds ---
    if not is_timestamp_valid(raw_timestamp):
        print(f"[WARN] Rejecting telemetry due to invalid timestamp: {raw_timestamp}")
        print(f"Human-readable: {datetime.fromtimestamp(raw_timestamp, tz=timezone.utc)}")

        return {
            "statusCode": 200,
            "body": json.dumps({
                "message": "Rejected due to invalid timestamp",
                "timestamp_received": raw_timestamp
            })
        }

    # Convert to nanoseconds ONLY after validation
    timestamp_ns = raw_timestamp * 1_000_000_000

    # 6. Write to InfluxDB
    client = InfluxDBClient(
        url=os.environ["INFLUX_URL"],
        token=os.environ["INFLUX_TOKEN"],
        org=os.environ["INFLUX_ORG"]
    )

    write_api = client.write_api(write_options=SYNCHRONOUS)

    point = (
        Point("telemetry")
        .tag("device_id", device_id)
        .tag("room", room)
        .field("temperature_c", float(temp_c))
        .field("humidity_pct", float(humidity))
        .field("mq9_raw", int(mq9_raw))
        .field("mq9_ppm", float(mq9_ppm))
        .field("rssi", int(rssi))
        .time(timestamp_ns, WritePrecision.NS)
    )

    write_api.write(
        bucket=os.environ["INFLUX_BUCKET"],
        record=point
    )

    print("Telemetry written to InfluxDB.")

    return {
        "statusCode": 200,
        "body": json.dumps({"message": "Success"})
    }
