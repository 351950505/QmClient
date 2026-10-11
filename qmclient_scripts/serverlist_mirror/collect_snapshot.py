#!/usr/bin/env python3
"""采集官方列表；全部失败时不改变上一份快照或它的时间。"""

import argparse
import hashlib
import json
import os
import time
import urllib.request
from dataclasses import dataclass
from email.utils import parsedate_to_datetime
from pathlib import Path

SOURCES = tuple(f"https://master{i}.ddnet.org/ddnet/15/servers.json" for i in range(1, 5))
MAX_BODY = 8 * 1024 * 1024
MAX_AGE = 24 * 60 * 60
TIMEOUT = 7


class SnapshotError(ValueError):
    pass


@dataclass(frozen=True)
class Response:
    body: bytes
    headers: dict


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "QmClient-serverlist-snapshot/1", "Accept": "application/json"})
    deadline = time.monotonic() + TIMEOUT
    with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
        if response.status != 200 or not response.geturl().startswith("https://"):
            raise SnapshotError("expected a successful HTTPS response")
        # read1 每次至多一次底层读取，配合 socket 超时，单源最长约 14 秒。
        chunks = []
        size = 0
        while True:
            if time.monotonic() >= deadline:
                raise TimeoutError("response body deadline exceeded")
            chunk = response.read1(min(65536, MAX_BODY + 1 - size))
            if not chunk:
                break
            chunks.append(chunk)
            size += len(chunk)
            if size > MAX_BODY:
                raise SnapshotError("response exceeds 8 MiB")
        return Response(b"".join(chunks), dict(response.headers.items()))


def payload_time(headers, now):
    headers = {key.lower(): value for key, value in headers.items()}
    try:
        age = int(headers.get("age", "0"))
        if age < 0:
            raise ValueError("negative Age")
        elapsed = 0
        if "date" in headers:
            date = int(parsedate_to_datetime(headers["date"]).timestamp())
            if date > now + 300:
                raise ValueError("future Date")
            elapsed = max(0, now - date)
        effective_age = age + elapsed
        if "last-modified" in headers:
            modified = int(parsedate_to_datetime(headers["last-modified"]).timestamp())
            if modified > now + 300:
                raise ValueError("future Last-Modified")
            effective_age = max(effective_age, max(0, now - modified))
    except (TypeError, ValueError, OverflowError) as error:
        raise SnapshotError(f"invalid freshness headers: {error}") from error
    if effective_age > MAX_AGE:
        raise SnapshotError("upstream payload is older than 24 hours")
    return now - effective_age


def validate(response, now):
    if len(response.body) > MAX_BODY:
        raise SnapshotError("response exceeds 8 MiB")
    try:
        document = json.loads(response.body)
    except (ValueError, UnicodeError) as error:
        raise SnapshotError("invalid JSON") from error
    if not isinstance(document, dict) or not isinstance(document.get("servers"), list) or not document["servers"]:
        raise SnapshotError("missing or empty servers array")
    for server in document["servers"]:
        if not isinstance(server, dict) or not isinstance(server.get("info"), dict):
            raise SnapshotError("invalid server info structure")
        addresses = server.get("addresses")
        if not isinstance(addresses, list) or not addresses or not all(isinstance(item, str) and item for item in addresses):
            raise SnapshotError("invalid server addresses")
        if "location" in server and not isinstance(server["location"], str):
            raise SnapshotError("invalid server location")
    updated = payload_time(response.headers, now)
    # 若上游已有快照字段，不能通过再次采集给旧内容重新计时。
    previous = document.get("qm_snapshot_updated_at")
    if previous is not None:
        if type(previous) is not int or previous < now - MAX_AGE or previous > now + 300:
            raise SnapshotError("invalid or expired upstream snapshot timestamp")
        updated = min(updated, previous)
    document["qm_snapshot_updated_at"] = updated
    body = (json.dumps(document, ensure_ascii=False, separators=(",", ":"), allow_nan=False) + "\n").encode("utf-8")
    if len(body) > MAX_BODY:
        raise SnapshotError("serialized snapshot exceeds 8 MiB")
    return body, updated, len(document["servers"])


def collect(output, fetcher=fetch, now=None, sources=SOURCES):
    """仅替换 HTTP 边界；校验、时间计算与发布数据使用同一生产实现。"""
    now = int(time.time()) if now is None else now
    failures = []
    for source in sources:
        try:
            body, updated, count = validate(fetcher(source), now)
        except (OSError, ValueError) as error:
            failures.append(f"{source}: {error}")
            continue
        metadata = {"source": source, "count": count, "observed_at": now, "updated_at": updated, "sha256": hashlib.sha256(body).hexdigest()}
        output = Path(output)
        output.mkdir(parents=True, exist_ok=True)
        for name, content in (("servers.json", body), ("snapshot-meta.json", (json.dumps(metadata, indent=2) + "\n").encode())):
            temporary = output / (name + ".tmp")
            temporary.write_bytes(content)
            os.replace(temporary, output / name)
        return metadata
    raise SnapshotError("all official sources failed; existing snapshot retained\n" + "\n".join(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("tmp/serverlist-snapshot"))
    args = parser.parse_args()
    try:
        print(json.dumps(collect(args.output), ensure_ascii=False))
    except SnapshotError as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
