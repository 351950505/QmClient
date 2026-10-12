#!/usr/bin/env python3
"""采集官方列表；全部失败时不改变上一份快照或它的时间。"""

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
import time
from dataclasses import dataclass
from email.parser import BytesParser
from email.utils import parsedate_to_datetime
from pathlib import Path

from serverlist_contract import validate_document

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
    # Actions Ubuntu 自带 curl；max-time 也约束 DNS，避免 Python 系统解析无界等待。
    with tempfile.TemporaryDirectory(prefix="qm-serverlist-fetch-") as directory:
        header_path = Path(directory) / "headers"
        body_path = Path(directory) / "body"
        command = ["curl", "--fail", "--silent", "--show-error", "--proto", "=https",
                   "--connect-timeout", str(TIMEOUT), "--max-time", "14", "--max-filesize", str(MAX_BODY),
                   "--user-agent", "QmClient-serverlist-snapshot/1", "--header", "Accept: application/json",
                   "--dump-header", str(header_path), "--output", str(body_path), "--write-out", "%{http_code}", url]
        try:
            result = subprocess.run(command, capture_output=True, timeout=15, check=False)
        except subprocess.TimeoutExpired as error:
            raise SnapshotError("HTTP process exceeded deadline") from error
        if result.returncode or result.stdout.strip() != b"200":
            raise SnapshotError("HTTP fetch failed: " + result.stderr.decode("utf-8", errors="replace").strip())
        if body_path.stat().st_size > MAX_BODY:
            raise SnapshotError("response exceeds 8 MiB")
        # 代理 CONNECT 和 1xx 可以先于最终响应，只使用最后一块响应头。
        blocks = header_path.read_bytes().replace(b"\r\n", b"\n").strip().split(b"\n\n")
        lines = blocks[-1].split(b"\n", 1)
        if len(lines) != 2 or not lines[0].startswith(b"HTTP/"):
            raise SnapshotError("missing final HTTP response headers")
        headers = BytesParser().parsebytes(lines[1])
        return Response(body_path.read_bytes(), dict(headers.items()))


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
    validate_document(document)
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
