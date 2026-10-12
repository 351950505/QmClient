import copy
import json
import unittest

from serverlist_contract import validate_document, validate_serverlist


def server():
    return {"addresses": ["tw-0.6+udp://127.0.0.1:8303"], "location": "eu:de", "info": {
        "name": "Server", "game_type": "DDRace", "version": "0.6", "map": {"name": "Map"},
        "max_clients": 16, "max_players": 16, "passworded": False, "clients": []}}


class ServerlistContract(unittest.TestCase):
    def test_valid_payload_reports_usable_count_and_keeps_extra_fields(self):
        document = {"servers": [server()], "extra": {"future": True}}
        before = copy.deepcopy(document)
        self.assertEqual(validate_document(document), 1)
        self.assertEqual(validate_serverlist(json.dumps(document).encode()), 1)
        self.assertEqual(document, before)

    def test_invalid_info_is_skipped_without_removing_raw_entry(self):
        document = {"servers": [{"addresses": [123], "info": {}}, server()]}
        self.assertEqual(validate_document(document), 1)
        self.assertEqual(len(document["servers"]), 2)

    def test_missing_required_info_rejects_all_unusable_payload(self):
        for key in server()["info"]:
            value = server()
            del value["info"][key]
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_document({"servers": [value]})

    def test_info_types_control_characters_and_capacity_are_checked(self):
        for key, bad in (("max_clients", True), ("max_players", -1), ("max_clients", 2**31),
                         ("passworded", 0), ("name", "bad\nname"), ("game_type", None),
                         ("clients", {}), ("client_score_kind", 3), ("client_score_kind", None)):
            value = server()
            value["info"][key] = bad
            with self.subTest(key=key, bad=bad), self.assertRaises(ValueError):
                validate_document({"servers": [value]})

    def test_client_fields_and_capacity_relations_are_checked(self):
        client = {"name": "Tee", "clan": "", "country": -1, "score": 0, "is_player": True}
        value = server()
        value["info"]["clients"] = [client]
        self.assertEqual(validate_document({"servers": [value]}), 1)
        for field in client:
            invalid = copy.deepcopy(value)
            del invalid["info"]["clients"][0][field]
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate_document({"servers": [invalid]})
        value["info"]["max_players"] = 0
        with self.assertRaises(ValueError):
            validate_document({"servers": [value]})

    def test_only_numeric_supported_address_with_nonzero_port_counts(self):
        for address in ("garbage", "TW-0.6+UDP://127.0.0.1:8303", "tw-0.6+udp://ddnet.org:8303", "tw-0.6+tcp://127.0.0.1:8303",
                        "tw-0.6+udp://127.0.0.1", "tw-0.6+udp://127.0.0.1:0", "tw-0.6+udp://127.0.0.1:65536"):
            value = server()
            value["addresses"] = [address]
            with self.subTest(address=address), self.assertRaises(ValueError):
                validate_document({"servers": [value]})
        value = server()
        value["addresses"] = ["unsupported", "tw-0.7+udp://[::1]:8303"]
        self.assertEqual(validate_document({"servers": [value]}), 1)

    def test_six_protocol_preference_matches_client(self):
        value = server()
        value["addresses"] = ["tw-0.6+udp://bad", "tw-0.7+udp://127.0.0.1:8303"]
        with self.assertRaises(ValueError):
            validate_document({"servers": [value]})

    def test_structural_failure_invalidates_otherwise_valid_list(self):
        for invalid in ({"addresses": False, "info": {}}, {"addresses": [], "location": "cn", "info": {}},
                        {"addresses": [], "location": None, "info": {}},
                        {"addresses": [123], "info": server()["info"]}):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                validate_document({"servers": [server(), invalid]})

    def test_empty_bad_json_and_wrong_root_are_rejected(self):
        for body in (b"<html>bad</html>", b"{}", b"[]", b'{"servers":[]}'):
            with self.subTest(body=body), self.assertRaises(ValueError):
                validate_serverlist(body)


if __name__ == "__main__":
    unittest.main()
