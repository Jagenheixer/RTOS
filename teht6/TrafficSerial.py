import re
import time

import serial


class TrafficSerial:
    ROBOT_LIBRARY_SCOPE = "SUITE"

    def __init__(self):
        self._connection = None

    def open_serial_port(self, port="COM5", baudrate=115200):
        if self._connection is not None and self._connection.is_open:
            raise RuntimeError("Serial port is already open")

        self._connection = serial.Serial(
            port=port,
            baudrate=int(baudrate),
            timeout=2,
        )
        time.sleep(2)
        self._connection.reset_input_buffer()

    def close_serial_port(self):
        if self._connection is not None and self._connection.is_open:
            self._connection.close()

    def send_command(self, command):
        if self._connection is None or not self._connection.is_open:
            raise RuntimeError("Serial port is not open")

        self._connection.reset_input_buffer()
        for value in (command + "X").encode("ascii"):
            self._connection.write(bytes((value,)))
            self._connection.flush()
            time.sleep(0.015)
        response = self._connection.read_until(b"X").decode("ascii", errors="replace")
        matches = re.findall(r"-?\d+(?:ms)?X", response)
        if not matches:
            raise AssertionError(
                f"No command response received for {command!r}; received {response!r}"
            )
        return matches[-1]
