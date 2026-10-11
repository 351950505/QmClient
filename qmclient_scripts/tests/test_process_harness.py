"""进程控制管道的就绪、失败与超时边界。"""

from __future__ import annotations

import errno
import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest import mock

from qmclient_scripts.integration import process_harness


class ProcessFifoTest(unittest.TestCase):
	def setUp(self):
		self.process = process_harness.Process.__new__(process_harness.Process)
		self.process.name = "client"
		self.process._fifo_path = "test.fifo"
		self.process._fifo = None
		self.process._process = mock.Mock()
		self.process._process.poll.return_value = None
		self.process._lines = ["client: initialization failed"]
		self.api = SimpleNamespace(name="posix", open=mock.Mock(), fdopen=mock.Mock(), set_blocking=mock.Mock(), O_WRONLY=os.O_WRONLY, O_NONBLOCK=getattr(os, "O_NONBLOCK", 2048))
		patcher = mock.patch.object(process_harness, "os", self.api)
		patcher.start()
		self.addCleanup(patcher.stop)
		opener = mock.patch.object(process_harness, "open", create=True)
		opener.start()
		self.addCleanup(opener.stop)

	def test_missing_reader_times_out_with_client_output(self):
		self.api.open.side_effect = OSError(errno.ENXIO, "no reader")
		with mock.patch.object(process_harness.time, "monotonic", side_effect=[0, 1]):
			with self.assertRaisesRegex(TimeoutError, "initialization failed"):
				self.process._open_fifo(timeout=0.1)
		self.assertIsNone(self.process._fifo)

	def test_exited_client_fails_before_waiting_for_reader(self):
		self.api.open.side_effect = OSError(errno.ENXIO, "no reader")
		self.process._process.poll.return_value = 7
		with self.assertRaisesRegex(RuntimeError, "initialization failed"):
			self.process._open_fifo()

	def test_recreated_fifo_retries_and_sends_utf8_command(self):
		self.api.open.side_effect = [FileNotFoundError(), OSError(errno.ENXIO, "no reader"), 42]
		with mock.patch.object(process_harness.time, "sleep"):
			self.process.command("echo 中文")
		self.api.fdopen.return_value.write.assert_called_once_with("echo 中文\n")
		self.api.set_blocking.assert_called_once_with(42, True)


@unittest.skipIf(os.name == "nt", "requires native POSIX FIFOs")
class NativeProcessFifoTest(unittest.TestCase):
	def test_command_reaches_real_reader(self):
		import sys
		import tempfile

		root = Path(__file__).resolve().parents[2] / "tmp"
		root.mkdir(exist_ok=True)
		with tempfile.TemporaryDirectory(dir=root) as directory:
			program = "import sys; p=sys.argv[-1].split(' ', 1)[1]; f=open(p, encoding='utf-8'); print(f.readline().strip(), flush=True)"
			process = process_harness.Process("reader", [sys.executable, "-c", program], Path(directory), fifo_command="pipe")
			try:
				process.command("echo 中文")
				process.wait_for(lambda line: line == "echo 中文", "UTF-8 command", 5)
				self.assertEqual(process.wait_for_exit(5), 0)
			finally:
				process.stop()


if __name__ == "__main__":
	unittest.main()
