"""读取当前平台实际参与编译的翻译单元。"""

from __future__ import annotations

import json
from pathlib import Path


def compilation_units(database: Path) -> set[Path]:
	entries = json.loads(database.read_text(encoding="utf-8-sig"))
	if not isinstance(entries, list):
		raise ValueError(f"invalid compilation database: {database}")
	units = set()
	for entry in entries:
		if not isinstance(entry, dict) or not isinstance(entry.get("file"), str) or not isinstance(entry.get("directory"), str):
			raise ValueError(f"invalid compilation database entry: {database}")
		directory = Path(entry["directory"])
		if not directory.is_absolute():
			directory = database.parent / directory
		file = Path(entry["file"])
		units.add((file if file.is_absolute() else directory / file).resolve())
	return units
