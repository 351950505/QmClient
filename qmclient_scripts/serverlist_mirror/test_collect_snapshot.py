import hashlib
import json
import subprocess
import tempfile
import unittest
from unittest import mock
from email.utils import formatdate
from pathlib import Path

from collect_snapshot import MAX_AGE, MAX_BODY, Response, SnapshotError, collect

NOW = 1_800_000_000


def reply(document=None, headers=None):
    if document is None:
        document = {"servers": [{"addresses": ["tw-0.6+udp://127.0.0.1:8303"], "info": {"name": "Server"}}], "extra": {"preserved": True}}
    return Response(json.dumps(document).encode(), headers or {})


class SnapshotCollection(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.output = Path(self.directory.name)

    def run_collect(self, response):
        return collect(self.output, lambda url: response, NOW, ["https://official.test/list"])

    def test_valid_snapshot_preserves_root_and_records_payload_hash(self):
        metadata = self.run_collect(reply())
        body = (self.output / "servers.json").read_bytes()
        document = json.loads(body)
        self.assertEqual(document["extra"], {"preserved": True})
        self.assertEqual(document["qm_snapshot_updated_at"], NOW)
        self.assertEqual(metadata["sha256"], hashlib.sha256(body).hexdigest())
        self.assertEqual(metadata["count"], 1)

    def test_invalid_json_and_empty_list_advance_to_valid_source(self):
        responses = iter([Response(b"<html>bad</html>", {}), reply({"servers": []}), reply()])
        metadata = collect(self.output, lambda url: next(responses), NOW, ["https://one", "https://two", "https://three"])
        self.assertEqual(metadata["source"], "https://three")

    def test_all_failures_preserve_existing_body_and_timestamp(self):
        self.run_collect(reply())
        saved = {path.name: path.read_bytes() for path in self.output.iterdir()}
        def fail(url):
            raise TimeoutError("timed out")
        with self.assertRaises(SnapshotError):
            collect(self.output, fail, NOW + 500, ["https://one", "https://two"])
        self.assertEqual(saved, {path.name: path.read_bytes() for path in self.output.iterdir()})

    def test_http_process_deadline_is_converted_and_other_sources_are_attempted(self):
        self.run_collect(reply())
        saved = (self.output / "servers.json").read_bytes()
        with mock.patch("collect_snapshot.subprocess.run", side_effect=subprocess.TimeoutExpired("curl", 15)) as boundary:
            with self.assertRaisesRegex(SnapshotError, "all official sources failed"):
                collect(self.output, now=NOW + 100, sources=["https://one", "https://two"])
        self.assertEqual(boundary.call_count, 2)
        self.assertEqual((self.output / "servers.json").read_bytes(), saved)

    def test_last_modified_dominates_fresh_response_date(self):
        metadata = self.run_collect(reply(headers={"Date": formatdate(NOW, usegmt=True), "Age": "10", "Last-Modified": formatdate(NOW - 600, usegmt=True)}))
        self.assertEqual(metadata["updated_at"], NOW - 600)

    def test_age_includes_transit_time_without_last_modified(self):
        metadata = self.run_collect(reply(headers={"date": formatdate(NOW - 20, usegmt=True), "age": "40"}))
        self.assertEqual(metadata["updated_at"], NOW - 60)

    def test_historical_snapshot_timestamp_is_not_refreshed(self):
        document = json.loads(reply().body)
        document["qm_snapshot_updated_at"] = NOW - 700
        self.assertEqual(self.run_collect(reply(document))["updated_at"], NOW - 700)

    def test_oversized_response_is_rejected_without_writing(self):
        with self.assertRaises(SnapshotError):
            self.run_collect(Response(b" " * (MAX_BODY + 1), {}))
        self.assertEqual(list(self.output.iterdir()), [])

    def test_expired_payload_and_invalid_time_headers_are_rejected(self):
        for headers in ({"Age": str(MAX_AGE + 1)}, {"Age": "-1"}, {"Age": "bad"}, {"Last-Modified": formatdate(NOW - MAX_AGE - 1, usegmt=True)}, {"Date": formatdate(NOW + 301, usegmt=True)}):
            with self.subTest(headers=headers), self.assertRaises(SnapshotError):
                self.run_collect(reply(headers=headers))

    def test_missing_info_or_bad_address_rejects_whole_payload(self):
        for server in ({"addresses": ["tw-0.6+udp://127.0.0.1:8303"]}, {"addresses": [123], "info": {}}, {"addresses": [], "info": {}}):
            with self.subTest(server=server), self.assertRaises(SnapshotError):
                self.run_collect(reply({"servers": [server]}))


if __name__ == "__main__":
    unittest.main()
