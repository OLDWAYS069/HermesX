#!/usr/bin/env python3
import argparse
import concurrent.futures
import http.client
import ipaddress
import json
import os
import socket
import sys
from typing import Optional, Tuple


DEFAULT_DISCOVERY_PATH = "/update-info"
DEFAULT_UPLOAD_PATH = "/upload-update-bin"
UPLOAD_CHUNK_SIZE = 64 * 1024


def fetch_update_info(host: str, port: int, timeout: float) -> Optional[dict]:
    conn = None
    try:
        conn = http.client.HTTPConnection(host, port, timeout=timeout)
        conn.request("GET", DEFAULT_DISCOVERY_PATH)
        res = conn.getresponse()
        body = res.read()
        if res.status != 200:
            return None
        payload = json.loads(body.decode("utf-8"))
        if not payload.get("hermesx_update"):
            return None
        payload["_host"] = host
        payload["_port"] = port
        return payload
    except Exception:
        return None
    finally:
        if conn:
            conn.close()


def discover_via_mdns(port: int, timeout: float) -> Optional[dict]:
    for host in ("Meshtastic.local", "meshtastic.local"):
        info = fetch_update_info(host, port, timeout)
        if info:
            return info
    return None


def get_local_ipv4() -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.connect(("8.8.8.8", 80))
        return sock.getsockname()[0]
    finally:
        sock.close()


def probe_host(host: str, port: int, timeout: float) -> Optional[dict]:
    return fetch_update_info(host, port, timeout)


def discover_on_local_subnet(port: int, timeout: float) -> Optional[dict]:
    local_ip = get_local_ipv4()
    network = ipaddress.ip_network(f"{local_ip}/24", strict=False)
    candidates = [str(ip) for ip in network.hosts() if str(ip) != local_ip]

    with concurrent.futures.ThreadPoolExecutor(max_workers=48) as pool:
        future_map = {pool.submit(probe_host, host, port, timeout): host for host in candidates}
        for future in concurrent.futures.as_completed(future_map):
            info = future.result()
            if info:
                pool.shutdown(wait=False, cancel_futures=True)
                return info
    return None


def discover_device(port: int, timeout: float) -> dict:
    info = discover_via_mdns(port, timeout)
    if info:
        return info

    info = discover_on_local_subnet(port, timeout)
    if info:
        return info

    raise SystemExit("找不到 HermesX 更新裝置，請確認裝置已進入 WiFi更新 > 開始更新，且電腦與裝置在同一個區網。")


def upload_firmware(host: str, port: int, upload_path: str, firmware_path: str):
    size = os.path.getsize(firmware_path)
    filename = os.path.basename(firmware_path)
    conn = http.client.HTTPConnection(host, port, timeout=30)
    try:
        conn.putrequest("PUT", upload_path)
        conn.putheader("Content-Length", str(size))
        conn.putheader("Content-Type", "application/octet-stream")
        conn.putheader("X-Hermes-Filename", filename)
        conn.endheaders()

        sent = 0
        with open(firmware_path, "rb") as fh:
            while True:
                chunk = fh.read(UPLOAD_CHUNK_SIZE)
                if not chunk:
                    break
                conn.send(chunk)
                sent += len(chunk)
                pct = int((sent * 100) / size)
                sys.stdout.write(f"\rWiFi upload {pct:3d}% ({sent}/{size})")
                sys.stdout.flush()

        res = conn.getresponse()
        body = res.read().decode("utf-8", errors="replace")
        sys.stdout.write("\n")
        if 200 <= res.status < 300:
            print(f"上傳完成: http://{host}:{port}{upload_path}")
            if body.strip():
                print(body.strip())
            return
        raise SystemExit(f"上傳失敗: HTTP {res.status}\n{body}")
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser(description="Discover HermesX update mode over WiFi and upload firmware.bin")
    parser.add_argument("firmware", help="Path to firmware.bin")
    parser.add_argument("--host", help="Device host/IP. Omit to auto-discover.")
    parser.add_argument("--port", type=int, default=80, help="HTTP port, default 80")
    parser.add_argument("--timeout", type=float, default=0.35, help="Discovery timeout per host in seconds")
    args = parser.parse_args()

    firmware_path = os.path.abspath(args.firmware)
    if not os.path.exists(firmware_path):
        raise SystemExit(f"找不到韌體: {firmware_path}")

    if args.host:
        info = fetch_update_info(args.host, args.port, max(args.timeout, 1.0))
        if not info:
            raise SystemExit(f"無法連到更新裝置: {args.host}:{args.port}")
    else:
        info = discover_device(args.port, args.timeout)

    host = info["_host"]
    port = info["_port"]
    upload_path = info.get("uploadPath") or DEFAULT_UPLOAD_PATH
    version = info.get("version") or "unknown"
    ssid = info.get("ssid") or ""
    ip = info.get("ip") or host

    print(f"找到裝置: {host} ({ip})")
    if ssid:
        print(f"SSID: {ssid}")
    print(f"版本: {version}")
    print(f"上傳路徑: {upload_path}")

    upload_firmware(host, port, upload_path, firmware_path)


if __name__ == "__main__":
    main()
