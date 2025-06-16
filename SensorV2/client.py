import argparse
import socket
import struct
import threading
import time
import random
import sys
from dataclasses import dataclass
from typing import Tuple

# Protocol constants (host byte order little-endian)
CONNECTION_PORT: int = 4040  # TCP port for sensor registration
SEND_PORT: int = 5050        # UDP port for regular payloads
ALERT_PORT: int = 6060       # TCP port for alert notifications
TICK: int = 2                # Interval between payloads (seconds)

# Alert thresholds
MAX_ALERT_TEMPERATURE: int = 50  # Celsius threshold for temperature alert
MAX_ALERT_HUMIDITY: int = 60     # Percent threshold for humidity alert
MIN_ALERT_AIR_QUALITY: int = 10  # Minimum acceptable air quality index

# Struct formats (little-endian, packed)
# Sensor: ID (1B) + padding (16B) = 17 bytes
SENSOR_STRUCT_FORMAT: str = '<B16s'
# Alert: Type (4B) + ID (1B) + padding (16B) = 21 bytes
ALERT_STRUCT_FORMAT: str = '<I B16s'
# Payload: Timestamp (8B) + Temperature (1B) + Humidity (1B) + AirQuality (1B) = 11 bytes
PAYLOAD_STRUCT_FORMAT: str = '<Q B B B'

# Calculated sizes
SENSOR_STRUCT_SIZE: int = struct.calcsize(SENSOR_STRUCT_FORMAT)
ALERT_STRUCT_SIZE: int = struct.calcsize(ALERT_STRUCT_FORMAT)
PAYLOAD_STRUCT_SIZE: int = struct.calcsize(PAYLOAD_STRUCT_FORMAT)

# Human-readable payload print template
PAYLOAD_TEMPLATE: str = "{sensor_id} at {timestamp}: {temperature} C {humidity} H {airQuality} %%"

# Alert message types
ALERT: int = 0       # Outgoing alert notification
REACTIVATE: int = 1  # Incoming reactivation confirmation

@dataclass(frozen=True)
class Sensor:
    """
    Represents a sensor with an ID and server address.
    Attributes:
        sensor_id: Unique sensor identifier (0-255).
        address: Tuple of server IP and port.
    """
    sensor_id: int
    address: Tuple[str, int]

@dataclass(frozen=True)
class SensorPayload:
    """
    Telemetry data from the sensor.
    Attributes:
        timestamp: Unix epoch seconds when the payload was generated.
        temperature: Current temperature reading.
        humidity: Current humidity reading.
        airQuality: Current air quality index.
    """
    timestamp: int
    temperature: int
    humidity: int
    airQuality: int


def CheckArgs() -> Tuple[int, str]:
    """
    Parses and validates command-line arguments.
    Returns:
        Tuple containing sensor ID and server IPv4 string.
    Exits on invalid input.
    """
    parser = argparse.ArgumentParser(description="Sensor client application")
    parser.add_argument('sensor_id', type=int, help='Sensor ID (0-255)')
    parser.add_argument('server_ip', type=str, help='Server IPv4 address')
    args = parser.parse_args()
    if not (0 <= args.sensor_id <= 0xFF):
        print("ID MUST BE BETWEEN 0 AND 255", file=sys.stderr)
        sys.exit(1)
    return args.sensor_id, args.server_ip


def RegisterToServer(sensor: Sensor, server_ip: str) -> None:
    """
    Establishes a TCP connection to the central server and sends the Sensor struct.
    """
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        try:
            sock.connect((server_ip, CONNECTION_PORT))
        except socket.error as err:
            print(f"Connection failed: {err}", file=sys.stderr)
            sys.exit(1)
        packed_sensor: bytes = struct.pack(
            SENSOR_STRUCT_FORMAT,
            sensor.sensor_id,
            b'\x00' * 16  # padding, server will ignore
        )
        bytes_sent = 0
        while bytes_sent < SENSOR_STRUCT_SIZE:
            sent = sock.send(packed_sensor[bytes_sent:])
            if sent <= 0:
                print("Failed to send complete Sensor struct", file=sys.stderr)
                sys.exit(1)
            bytes_sent += sent


def CreateRandomPayload() -> SensorPayload:
    """
    Constructs a SensorPayload with random values in valid ranges.
    """
    return SensorPayload(
        timestamp=int(time.time()),
        temperature=random.randint(0, 60),
        humidity=random.randint(0, 70),
        airQuality=random.randint(0, 100)
    )


def ShouldAlert(payload: SensorPayload) -> bool:
    """
    Determines if a payload exceeds alert thresholds.
    """
    return (
        payload.temperature > MAX_ALERT_TEMPERATURE or
        payload.humidity > MAX_ALERT_HUMIDITY or
        payload.airQuality < MIN_ALERT_AIR_QUALITY
    )


def AlertWait(sensor: Sensor) -> None:
    """
    Sends an ALERT message over TCP and waits for REACTIVATE response.
    Exits on any communication error or incorrect response.
    """
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        try:
            sock.connect((sensor.address[0], ALERT_PORT))
        except socket.error as err:
            print(f"Alert connection failed: {err}", file=sys.stderr)
            sys.exit(1)
        # Pack SensorAlert: type (4B), id (1B), padding (16B)
        alert_msg: bytes = struct.pack(
            ALERT_STRUCT_FORMAT,
            ALERT,
            sensor.sensor_id,
            b'\x00' * 16
        )
        sent_total = 0
        while sent_total < ALERT_STRUCT_SIZE:
            sent = sock.send(alert_msg[sent_total:])
            if sent <= 0:
                print("Failed to send Alert struct", file=sys.stderr)
                sys.exit(1)
            sent_total += sent
        # Receive full reactivation struct
        buf = b''
        while len(buf) < ALERT_STRUCT_SIZE:
            chunk = sock.recv(ALERT_STRUCT_SIZE - len(buf))
            if not chunk:
                print("Connection closed during alert wait", file=sys.stderr)
                sys.exit(1)
            buf += chunk
        resp_type = struct.unpack_from('<I', buf, 0)[0]
        if resp_type != REACTIVATE:
            print("Unexpected response to alert", file=sys.stderr)
            sys.exit(1)


def main() -> None:
    """
    Main loop: registers the sensor, then periodically sends payloads.
    Triggers alert notifications when thresholds exceeded.
    """
    random.seed()
    sensor_id, server_ip = CheckArgs()
    sensor = Sensor(sensor_id, (server_ip, SEND_PORT))

    RegisterToServer(sensor, server_ip)
    print("Registration complete")

    print("Starting to send payloads...")
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        while True:
            payload = CreateRandomPayload()
            if ShouldAlert(payload):
                print("ALERT")
                thread = threading.Thread(target=AlertWait, args=(sensor,))
                thread.start()
                thread.join()

            packed_payload: bytes = struct.pack(
                PAYLOAD_STRUCT_FORMAT,
                payload.timestamp,
                payload.temperature,
                payload.humidity,
                payload.airQuality
            )
            sent_bytes = udp_socket.sendto(packed_payload, sensor.address)
            timestamp_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(payload.timestamp))
            print(PAYLOAD_TEMPLATE.format(
                sensor_id=sensor.sensor_id,
                timestamp=timestamp_str,
                temperature=payload.temperature,
                humidity=payload.humidity,
                airQuality=payload.airQuality
            ))

            if sent_bytes != PAYLOAD_STRUCT_SIZE:
                print("Partial payload sent", file=sys.stderr)
                break
            time.sleep(TICK)
    finally:
        udp_socket.close()


if __name__ == '__main__':
    main()