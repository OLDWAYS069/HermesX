#!/usr/bin/env python3
import argparse
import errno
import os
import select
import sys
import termios
import time


START1 = 0x94
START2 = 0xC3
XMODEM_SOH = 1
XMODEM_EOT = 4
XMODEM_ACK = 6
XMODEM_NAK = 21
XMODEM_CAN = 24
SPECIAL_NONCE_ONLY_NODES = 69421
TORADIO_WANT_CONFIG_TAG = 3
FROMRADIO_CONFIG_COMPLETE_TAG = 7
FROMRADIO_REBOOTED_TAG = 8
FROMRADIO_XMODEM_TAG = 12
FROMRADIO_LOG_RECORD_TAG = 6
FRAME_PATH = "/update/firmware.bin"
CHUNK_SIZE = 128


def encode_varint(value: int) -> bytes:
    out = bytearray()
    while True:
        b = value & 0x7F
        value >>= 7
        if value:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def decode_varint(data: bytes, offset: int):
    shift = 0
    value = 0
    while offset < len(data):
        b = data[offset]
        offset += 1
        value |= (b & 0x7F) << shift
        if not (b & 0x80):
            return value, offset
        shift += 7
    raise ValueError("incomplete varint")


def field_key(field_number: int, wire_type: int) -> bytes:
    return encode_varint((field_number << 3) | wire_type)


def encode_length_delimited(field_number: int, payload: bytes) -> bytes:
    return field_key(field_number, 2) + encode_varint(len(payload)) + payload


def encode_varint_field(field_number: int, value: int) -> bytes:
    return field_key(field_number, 0) + encode_varint(value)


def crc16_ccitt(buffer: bytes) -> int:
    crc16 = 0
    for byte in buffer:
        crc16 = ((crc16 >> 8) | ((crc16 << 8) & 0xFFFF)) & 0xFFFF
        crc16 ^= byte
        crc16 ^= (crc16 & 0xFF) >> 4
        crc16 ^= (crc16 << 12) & 0xFFFF
        crc16 ^= ((crc16 & 0xFF) << 5) & 0xFFFF
    return crc16 & 0xFFFF


def encode_xmodem_packet(control: int, seq: int = 0, crc16: int = 0, payload: bytes = b"") -> bytes:
    msg = bytearray()
    msg += encode_varint_field(1, control)
    if seq:
        msg += encode_varint_field(2, seq)
    if crc16:
        msg += encode_varint_field(3, crc16)
    if payload:
        msg += encode_length_delimited(4, payload)
    return bytes(msg)


def encode_to_radio_xmodem(control: int, seq: int = 0, crc16: int = 0, payload: bytes = b"") -> bytes:
    xmodem = encode_xmodem_packet(control, seq, crc16, payload)
    toradio = encode_length_delimited(5, xmodem)
    return bytes([START1, START2, (len(toradio) >> 8) & 0xFF, len(toradio) & 0xFF]) + toradio


def encode_to_radio_want_config(nonce: int) -> bytes:
    toradio = encode_varint_field(TORADIO_WANT_CONFIG_TAG, nonce)
    return bytes([START1, START2, (len(toradio) >> 8) & 0xFF, len(toradio) & 0xFF]) + toradio


def skip_field(data: bytes, offset: int, wire_type: int) -> int:
    if wire_type == 0:
        _, offset = decode_varint(data, offset)
        return offset
    if wire_type == 2:
        length, offset = decode_varint(data, offset)
        return offset + length
    raise ValueError(f"unsupported wire type {wire_type}")


def decode_xmodem_packet(data: bytes):
    offset = 0
    result = {"control": 0, "seq": 0, "crc16": 0, "payload": b""}
    while offset < len(data):
        key, offset = decode_varint(data, offset)
        field_number = key >> 3
        wire_type = key & 0x07
        if field_number in (1, 2, 3) and wire_type == 0:
            value, offset = decode_varint(data, offset)
            if field_number == 1:
                result["control"] = value
            elif field_number == 2:
                result["seq"] = value
            else:
                result["crc16"] = value
        elif field_number == 4 and wire_type == 2:
            length, offset = decode_varint(data, offset)
            result["payload"] = data[offset:offset + length]
            offset += length
        else:
            offset = skip_field(data, offset, wire_type)
    return result


def extract_fromradio_xmodem(data: bytes):
    offset = 0
    while offset < len(data):
        key, offset = decode_varint(data, offset)
        field_number = key >> 3
        wire_type = key & 0x07
        if field_number == 12 and wire_type == 2:
            length, offset = decode_varint(data, offset)
            return decode_xmodem_packet(data[offset:offset + length])
        offset = skip_field(data, offset, wire_type)
    return None


def decode_log_record(data: bytes):
    offset = 0
    result = {"message": "", "time": 0, "source": "", "level": 0}
    while offset < len(data):
        key, offset = decode_varint(data, offset)
        field_number = key >> 3
        wire_type = key & 0x07
        if field_number in (2, 4) and wire_type == 0:
            value, offset = decode_varint(data, offset)
            if field_number == 2:
                result["time"] = value
            else:
                result["level"] = value
            continue
        if field_number in (1, 3) and wire_type == 2:
            length, offset = decode_varint(data, offset)
            value = data[offset:offset + length].decode("utf-8", errors="replace")
            offset += length
            if field_number == 1:
                result["message"] = value
            else:
                result["source"] = value
            continue
        offset = skip_field(data, offset, wire_type)
    return result


def has_config_complete(data: bytes) -> bool:
    offset = 0
    while offset < len(data):
        key, offset = decode_varint(data, offset)
        field_number = key >> 3
        wire_type = key & 0x07
        if field_number == FROMRADIO_CONFIG_COMPLETE_TAG and wire_type == 0:
            return True
        offset = skip_field(data, offset, wire_type)
    return False


def describe_fromradio(data: bytes) -> str:
    offset = 0
    fields = []
    while offset < len(data):
        key, offset = decode_varint(data, offset)
        field_number = key >> 3
        wire_type = key & 0x07
        if field_number == FROMRADIO_CONFIG_COMPLETE_TAG and wire_type == 0:
            value, offset = decode_varint(data, offset)
            fields.append(f"config_complete({value})")
            continue
        if field_number == FROMRADIO_REBOOTED_TAG and wire_type == 0:
            value, offset = decode_varint(data, offset)
            fields.append(f"rebooted({value})")
            continue
        if field_number == FROMRADIO_XMODEM_TAG and wire_type == 2:
            length, offset = decode_varint(data, offset)
            packet = decode_xmodem_packet(data[offset:offset + length])
            offset += length
            fields.append(f"xmodem(control={packet['control']},seq={packet['seq']},len={len(packet['payload'])})")
            continue
        if field_number == FROMRADIO_LOG_RECORD_TAG and wire_type == 2:
            length, offset = decode_varint(data, offset)
            log_record = decode_log_record(data[offset:offset + length])
            offset += length
            source = log_record["source"] or "log"
            message = log_record["message"] or ""
            fields.append(f"log[{source}] {message}")
            continue
        offset = skip_field(data, offset, wire_type)
    return ", ".join(fields) if fields else "unknown"


def configure_serial(fd: int, baud: int = termios.B115200):
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = attrs[2] | termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[3] = 0
    attrs[4] = baud
    attrs[5] = baud
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


class FramedSerial:
    def __init__(self, port: str):
        self.port = port
        self.fd = self._open_port(port)
        self.buffer = bytearray()

    def _open_port(self, port: str):
        fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        configure_serial(fd)
        return fd

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def reconnect(self, timeout: float = 8.0):
        self.close()
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            try:
                self.fd = self._open_port(self.port)
                self.buffer.clear()
                time.sleep(0.2)
                return
            except OSError as exc:
                last_error = exc
                time.sleep(0.25)
        raise last_error if last_error else TimeoutError(f"timed out reopening {self.port}")

    def write(self, payload: bytes):
        total = 0
        while total < len(payload):
            try:
                total += os.write(self.fd, payload[total:])
            except OSError as exc:
                if exc.errno != errno.ENXIO:
                    raise
                self.reconnect()

    def read_frame(self, timeout: float):
        deadline = time.time() + timeout
        while time.time() < deadline:
            wait = max(0.0, deadline - time.time())
            try:
                readable, _, _ = select.select([self.fd], [], [], wait)
            except OSError as exc:
                if exc.errno != errno.ENXIO:
                    raise
                self.reconnect(timeout=max(0.5, deadline - time.time()))
                continue
            if readable:
                try:
                    chunk = os.read(self.fd, 4096)
                except OSError as exc:
                    if exc.errno != errno.ENXIO:
                        raise
                    self.reconnect(timeout=max(0.5, deadline - time.time()))
                    continue
                if chunk:
                    self.buffer.extend(chunk)
            while len(self.buffer) >= 4:
                if self.buffer[0] != START1 or self.buffer[1] != START2:
                    del self.buffer[0]
                    continue
                length = (self.buffer[2] << 8) | self.buffer[3]
                if len(self.buffer) < 4 + length:
                    break
                payload = bytes(self.buffer[4:4 + length])
                del self.buffer[:4 + length]
                return payload
        return None


def wait_for_device_ready(serial_link: FramedSerial, timeout: float = 2.0):
    deadline = time.time() + timeout
    saw_rebooted = False
    while time.time() < deadline:
        payload = serial_link.read_frame(max(0.1, deadline - time.time()))
        if payload is None:
            continue
        description = describe_fromradio(payload)
        print(f"\nRX {description}", file=sys.stderr)
        if "rebooted(" in description:
            saw_rebooted = True
    if saw_rebooted:
        time.sleep(0.8)


def wait_for_control(serial_link: FramedSerial, expected_controls, timeout: float):
    deadline = time.time() + timeout
    while time.time() < deadline:
        payload = serial_link.read_frame(max(0.1, deadline - time.time()))
        if payload is None:
            continue
        packet = extract_fromradio_xmodem(payload)
        if not packet:
            print(f"\nRX {describe_fromradio(payload)}", file=sys.stderr)
            continue
        control = packet["control"]
        if control in expected_controls:
            return packet
        print(f"\nRX xmodem unexpected control={control} seq={packet['seq']} len={len(packet['payload'])}", file=sys.stderr)
    raise TimeoutError(f"timed out waiting for controls {expected_controls}")


def wait_for_config_complete(serial_link: FramedSerial, timeout: float):
    deadline = time.time() + timeout
    while time.time() < deadline:
        payload = serial_link.read_frame(max(0.1, deadline - time.time()))
        if payload is None:
            continue
        if has_config_complete(payload):
            return
    raise TimeoutError("timed out waiting for config complete")


def autodetect_port():
    candidates = [p for p in os.listdir("/dev") if p.startswith("cu.usbmodem") or p.startswith("tty.usbmodem")]
    if len(candidates) == 1:
        return os.path.join("/dev", candidates[0])
    raise SystemExit("Please pass --port /dev/cu.usbmodemXXXX")


def main():
    parser = argparse.ArgumentParser(description="Upload HermesX firmware over USB update mode")
    parser.add_argument("firmware", help="Path to firmware.bin")
    parser.add_argument("--port", help="Serial port, e.g. /dev/cu.usbmodem14101")
    args = parser.parse_args()

    firmware_path = os.path.abspath(args.firmware)
    if not os.path.exists(firmware_path):
        raise SystemExit(f"Firmware not found: {firmware_path}")

    port = args.port or autodetect_port()
    size = os.path.getsize(firmware_path)
    filename = os.path.basename(firmware_path)
    start_payload = f"{FRAME_PATH}|{size}|{filename}".encode("utf-8")

    serial_link = FramedSerial(port)
    try:
        time.sleep(0.2)
        wait_for_device_ready(serial_link, timeout=2.0)

        start_packet = encode_to_radio_xmodem(XMODEM_SOH, seq=0, payload=start_payload)
        start_ok = False
        for attempt in range(3):
            if attempt:
                time.sleep(0.5)
            serial_link.write(start_packet)
            try:
                wait_for_control(serial_link, {XMODEM_ACK}, timeout=4.0)
                start_ok = True
                break
            except TimeoutError:
                print(f"\nNo ACK for start packet, retry {attempt + 1}/3", file=sys.stderr)
        if not start_ok:
            raise TimeoutError(f"timed out waiting for controls {{{XMODEM_ACK}}}")

        sent = 0
        seq = 1
        with open(firmware_path, "rb") as f:
            while True:
                chunk = f.read(CHUNK_SIZE)
                if not chunk:
                    break
                packet = encode_to_radio_xmodem(XMODEM_SOH, seq=seq, crc16=crc16_ccitt(chunk), payload=chunk)
                retries = 0
                while True:
                    serial_link.write(packet)
                    resp = wait_for_control(serial_link, {XMODEM_ACK, XMODEM_NAK, XMODEM_CAN}, timeout=8.0)
                    if resp["control"] == XMODEM_ACK:
                        sent += len(chunk)
                        pct = int((sent * 100) / size)
                        sys.stdout.write(f"\rUSB upload {pct:3d}% ({sent}/{size})")
                        sys.stdout.flush()
                        seq += 1
                        break
                    if resp["control"] == XMODEM_CAN:
                        raise SystemExit("\nDevice cancelled USB update")
                    retries += 1
                    if retries >= 10:
                        raise SystemExit("\nToo many retries while sending chunk")

        serial_link.write(encode_to_radio_xmodem(XMODEM_EOT))
        wait_for_control(serial_link, {XMODEM_ACK}, timeout=8.0)
        sys.stdout.write("\nUSB upload complete. Wait for device-side OTA write/apply flow.\n")
    finally:
        serial_link.close()


if __name__ == "__main__":
    main()
