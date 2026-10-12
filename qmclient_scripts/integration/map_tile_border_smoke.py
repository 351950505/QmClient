#!/usr/bin/env python3
"""通过真实 OpenGL 客户端验证地图边界 shader 失败后仍可启动和退出。"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

try:
	from qmclient_scripts.integration.process_harness import Process, log_message
	from qmclient_scripts.integration.startup_presentation_smoke import StartupEnvironment
except ModuleNotFoundError:
	from process_harness import Process, log_message
	from startup_presentation_smoke import StartupEnvironment


FALLBACK_MESSAGE = "Tile border shader unavailable, using unbuffered tile rendering."
COMPILE_ERROR = "Failed to compile shader file 'shader/tile_border.frag'."
LINK_ERROR = "Failed to link shader program '"
SHADER_ERRORS = ("Failed to read shader file '", "Failed to compile shader file '", LINK_ERROR, "Tile border shader is missing required uniforms")

# 两个阶段各自合法，但使用中的同名 varying 类型不同，必须在链接阶段失败。
LINK_FAILURE_VERTEX = """layout (location = 0) in vec2 inVertex;
uniform mat4x2 gPos;
uniform vec2 gOffset;
uniform vec2 gScale;
out vec3 QmLinkMismatch;
void main()
{
	vec2 VertexPos = inVertex * gScale + gOffset;
	gl_Position = vec4(gPos * vec4(VertexPos, 0.0, 1.0), 0.0, 1.0);
	QmLinkMismatch = vec3(inVertex, 1.0);
}
"""
LINK_FAILURE_FRAGMENT = """in vec4 QmLinkMismatch;
uniform vec4 gVertColor;
out vec4 FragClr;
void main()
{
	FragClr = QmLinkMismatch * gVertColor;
}
"""


def write_shader_override(env: StartupEnvironment, filename: str, source: str) -> None:
	# TYPE_ALL 先读取隔离存储目录；原始 data/shader 与其他场景不受影响。
	directory = env.temp_dir / "shader"
	directory.mkdir(exist_ok=True)
	(directory / filename).write_text(source, encoding="utf-8")


def start_client(env: StartupEnvironment, gl_version: str) -> Process:
	major, minor = gl_version.split(".")
	client = env.start([
		f"gfx_gl_major {major}",
		f"gfx_gl_minor {minor}",
		"gfx_gl_patch 0",
		"gfx_driver_is_blocked 0",
		"cl_download_skins 0",
		"cl_download_community_skins 0",
		"snd_enable 0",
		"logfile client-runtime.log",
		"minimize",
	])
	env.finish_startup()
	client.wait_for(lambda line: line.startswith("gfx: GPU renderer: "), "OpenGL renderer initialization", 30)
	# 查询最终配置，避免 context 初始化失败后悄悄降级到不测试边界 shader 的后端。
	start = len(client._lines)
	for command in ("gfx_backend", "gfx_gl_major", "gfx_gl_minor", "gfx_gl_patch", "echo qm_map_tile_border_config_checked"):
		client.command(command)
	client.wait_for(lambda line: "qm_map_tile_border_config_checked" in line, "graphics configuration response", 10)
	prefix = "config: Value: "
	values = [line.removeprefix(prefix) for raw in client._lines[start:] if (line := log_message(raw)).startswith(prefix)]
	if values != ["OpenGL", major, minor, "0"]:
		raise AssertionError(f"requested OpenGL {gl_version} was not retained: {values}")
	return client


def assert_no_shader_errors(client: Process) -> None:
	errors = [line for line in client._lines if any(error in line for error in SHADER_ERRORS)]
	if errors:
		raise AssertionError("unexpected shader errors:\n" + "\n".join(errors))


def smoke_compile_failure(env: StartupEnvironment, gl_version: str) -> None:
	write_shader_override(env, "tile_border.frag", "#error QmClient map border compile failure fixture\n")
	client = start_client(env, gl_version)
	client.wait_for(lambda line: COMPILE_ERROR in line, "injected border fragment compile failure", 10)
	client.wait_for(lambda line: FALLBACK_MESSAGE in line, "unbuffered tile rendering fallback", 10)
	env.quit()


def smoke_link_failure(env: StartupEnvironment, gl_version: str) -> None:
	write_shader_override(env, "tile_border.vert", LINK_FAILURE_VERTEX)
	write_shader_override(env, "tile_border.frag", LINK_FAILURE_FRAGMENT)
	client = start_client(env, gl_version)
	client.wait_for(lambda line: LINK_ERROR in line, "injected border program link failure", 10)
	client.wait_for(lambda line: FALLBACK_MESSAGE in line, "unbuffered tile rendering fallback", 10)
	if any("Failed to compile shader file '" in line for line in client._lines):
		raise AssertionError("link failure fixture failed at shader compilation instead")
	env.quit()


def smoke_normal_resources(env: StartupEnvironment, gl_version: str) -> None:
	client = start_client(env, gl_version)
	assert_no_shader_errors(client)
	if any(FALLBACK_MESSAGE in line for line in client._lines):
		raise AssertionError("normal border shader resources unexpectedly disabled tile buffering")
	env.quit()


SMOKE_TESTS = {
	"compile_failure": smoke_compile_failure,
	"link_failure": smoke_link_failure,
	"normal_resources": smoke_normal_resources,
}


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("build_dir", type=Path)
	parser.add_argument("test", choices=sorted(SMOKE_TESTS), nargs="?")
	parser.add_argument("--gl-version", choices=("3.0", "3.3"), default="3.0", help="3.0 checks the compatibility renderer; 3.3 checks the modern renderer")
	args = parser.parse_args()
	tests = {args.test: SMOKE_TESTS[args.test]} if args.test else SMOKE_TESTS
	failed = 0
	for name, test in tests.items():
		env: StartupEnvironment | None = None
		try:
			env = StartupEnvironment(args.build_dir)
			test(env, args.gl_version)
		except Exception as exc:
			failed += 1
			print(f"{name}: FAILED: {exc}", file=sys.stderr)
		else:
			print(f"{name}: passed (OpenGL {args.gl_version})")
		finally:
			if env is not None:
				env.close()
				print(f"artifacts: {env.temp_dir}")
	return 1 if failed else 0


if __name__ == "__main__":
	raise SystemExit(main())
