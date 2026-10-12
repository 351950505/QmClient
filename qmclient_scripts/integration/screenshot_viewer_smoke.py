#!/usr/bin/env python3
"""Windows 独立看图窗口真实进程冒烟，不构建、不启动游戏或读取玩家配置。

用法：python qmclient_scripts/integration/screenshot_viewer_smoke.py <build-dir>
检查原图打开、键盘缩放、多个窗口独立关闭及坏图错误退出。
临时图片和日志保留在 <build-dir>/tmp/tests/screenshot-viewer-*。
"""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import zlib


def wait_for(predicate, message: str):
	deadline = time.monotonic() + 15.0
	while time.monotonic() < deadline:
		value = predicate()
		if value:
			return value
		time.sleep(0.02)
	raise RuntimeError(message)


class Windows:
	def __init__(self):
		self.api = ctypes.WinDLL("user32", use_last_error=True)
		self.callback = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
		self.api.EnumWindows.argtypes = [self.callback, ctypes.c_void_p]
		self.api.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
		self.api.GetWindowTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int]
		self.api.IsWindowVisible.argtypes = [ctypes.c_void_p]
		self.api.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_ssize_t]
		self.api.MapVirtualKeyW.argtypes = [ctypes.c_uint, ctypes.c_uint]

	def title(self, hwnd):
		text = ctypes.create_unicode_buffer(1024)
		self.api.GetWindowTextW(hwnd, text, len(text))
		return text.value

	def find(self, process, expected_title):
		if process.poll() is not None:
			raise RuntimeError(f"viewer exited before creating its window: {process.returncode}")
		found = []

		@self.callback
		def visit(hwnd, _):
			pid = ctypes.c_ulong()
			self.api.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
			if pid.value == process.pid and self.api.IsWindowVisible(hwnd) and expected_title(self.title(hwnd)):
				found.append(hwnd)
			return 1

		self.api.EnumWindows(visit, None)
		return found[0] if found else None

	def post(self, hwnd, message, wparam=0, lparam=0):
		if not self.api.PostMessageW(hwnd, message, wparam, lparam):
			raise ctypes.WinError(ctypes.get_last_error())

	def key(self, hwnd, key):
		scan = self.api.MapVirtualKeyW(key, 0)
		self.post(hwnd, 0x0100, key, 1 | (scan << 16))
		self.post(hwnd, 0x0101, key, 1 | (scan << 16) | (3 << 30))


def write_png(path: Path):
	def chunk(name, data):
		return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data))

	rows = (b"\0" + bytes([240, 80, 120, 255]) * 512) * 256
	path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 256, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def start_viewer(executable: Path, directory: Path, image: Path, label: str):
	with (directory / f"{label}.log").open("wb") as log:
		return subprocess.Popen([str(executable), "--qm-screenshot-viewer", str(image), "Viewer load failed", "Unable to open image"], cwd=directory, stdout=log, stderr=subprocess.STDOUT)


def smoke_zoom_and_independent_close(executable, directory, windows):
	image = directory / "截图 viewer sample.png"
	write_png(image)
	first = start_viewer(executable, directory, image, "first")
	second = None
	try:
		hwnd = wait_for(lambda: windows.find(first, lambda title: title == f"{image.name} | 100% | QmClient"), "image window did not appear")
		windows.key(hwnd, 0x6B)  # VK_ADD
		wait_for(lambda: " | 120% | " in windows.title(hwnd), "zoom key did not change displayed scale")
		windows.key(hwnd, ord("0"))
		wait_for(lambda: " | 100% | " in windows.title(hwnd), "fit key did not restore scale")
		second = start_viewer(executable, directory, image, "second")
		other = wait_for(lambda: windows.find(second, lambda title: title.endswith(" | QmClient")), "second window did not appear")
		windows.post(hwnd, 0x0010)  # WM_CLOSE
		if first.wait(timeout=15) != 0 or second.poll() is not None:
			raise RuntimeError("closing one viewer affected another viewer or failed")
		windows.key(other, 0x1B)  # VK_ESCAPE
		if second.wait(timeout=15) != 0:
			raise RuntimeError("Escape did not close the second viewer cleanly")
	finally:
		for process in (first, second):
			if process is not None and process.poll() is None:
				process.terminate()
				process.wait(timeout=15)


def smoke_invalid_image(executable, directory, windows):
	image = directory / "invalid.png"
	image.write_bytes(b"not an image")
	process = start_viewer(executable, directory, image, "invalid")
	try:
		hwnd = wait_for(lambda: windows.find(process, lambda title: title == "Viewer load failed"), "invalid image did not show the error dialog")
		windows.post(hwnd, 0x0010)
		if process.wait(timeout=15) != 1:
			raise RuntimeError("invalid image did not exit with a load error")
	finally:
		if process.poll() is None:
			process.terminate()
			process.wait(timeout=15)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("build_dir", type=Path)
	args = parser.parse_args()
	if os.name != "nt":
		print("SKIP: this window automation uses Win32; viewport unit tests are portable")
		return
	build = args.build_dir.resolve()
	executable = build / "DDNet.exe"
	if not executable.is_file():
		raise RuntimeError(f"client binary not found: {executable}")
	artifacts = build / "tmp/tests"
	artifacts.mkdir(parents=True, exist_ok=True)
	directory = Path(tempfile.mkdtemp(prefix="screenshot-viewer-", dir=artifacts))
	print(f"Artifacts: {directory}")
	windows = Windows()
	smoke_zoom_and_independent_close(executable, directory, windows)
	smoke_invalid_image(executable, directory, windows)
	if any(path.is_dir() or path.suffix == ".cfg" for path in directory.iterdir()):
		raise RuntimeError("viewer unexpectedly created game configuration or storage directories")
	print("PASS: zoom, fit, independent close, Escape and invalid-image error")


if __name__ == "__main__":
	main()
