from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CMAKE = shutil.which("cmake")
PATCH_SCRIPT = REPO_ROOT / "cmake/PatchSDL3X11.cmake"
ORIGINAL_ALLOCATION = (
	"data->shm_info.shmid = shmget(IPC_PRIVATE, "
	"data->image->bytes_per_line * data->image->height, IPC_CREAT | 0777);"
)


@unittest.skipUnless(CMAKE, "CMake is required for the dependency patch contract")
class Sdl3PatchContractTest(unittest.TestCase):
	# 验证真实构建补丁的应用与缓存重入；不将源码合同称为 X11 运行时溢出测试。
	def setUp(self) -> None:
		tmp_root = REPO_ROOT / "tmp"
		tmp_root.mkdir(exist_ok=True)
		self.source_root = Path(tempfile.mkdtemp(prefix="sdl3-patch-contract-", dir=tmp_root))
		self.source_file = self.source_root / "src/video/x11/SDL_x11toolkit.c"
		self.source_file.parent.mkdir(parents=True)

	def apply_patch(self) -> subprocess.CompletedProcess[str]:
		return subprocess.run(
			[CMAKE, f"-DSDL3_SOURCE_DIR={self.source_root}", "-P", str(PATCH_SCRIPT)],
			capture_output=True,
			text=True,
			encoding="utf-8",
			timeout=30,
			check=False,
		)

	def test_patch_replaces_the_pinned_allocation_with_checked_size(self) -> None:
		self.source_file.write_text(ORIGINAL_ALLOCATION + "\n", encoding="utf-8")
		result = self.apply_patch()
		self.assertEqual(result.returncode, 0, result.stderr)
		patched = self.source_file.read_text(encoding="utf-8")
		self.assertIn("SDL_size_mul_check_overflow", patched)
		self.assertIn("shmget(IPC_PRIVATE, image_size, IPC_CREAT | 0777)", patched)
		self.assertNotIn(ORIGINAL_ALLOCATION, patched)

	def test_reapplying_the_patch_preserves_the_cached_source(self) -> None:
		self.source_file.write_text(ORIGINAL_ALLOCATION + "\n", encoding="utf-8")
		first = self.apply_patch()
		self.assertEqual(first.returncode, 0, first.stderr)
		patched = self.source_file.read_bytes()
		second = self.apply_patch()
		self.assertEqual(second.returncode, 0, second.stderr)
		self.assertEqual(self.source_file.read_bytes(), patched)

	def test_unknown_upstream_source_fails_without_overwriting_it(self) -> None:
		original = b"unexpected new SDL allocation implementation\n"
		self.source_file.write_bytes(original)
		result = self.apply_patch()
		self.assertNotEqual(result.returncode, 0)
		self.assertIn("unexpected SDL3 X11 source", result.stderr)
		self.assertEqual(self.source_file.read_bytes(), original)


if __name__ == "__main__":
	unittest.main()
