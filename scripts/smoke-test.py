"""Launch the real GLUT app; Windows also captures both scenes from its desktop."""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("executable", type=Path)
parser.add_argument("--screenshot-dir", type=Path)
args = parser.parse_args()
executable = args.executable.resolve()
output = args.screenshot_dir
if output:
    if sys.platform != "win32":
        parser.error("Windows desktop screenshot capture requires native Windows")
    output.mkdir(parents=True, exist_ok=True)

process = subprocess.Popen(
    [str(executable)], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    text=True, bufsize=1, env=dict(os.environ, CS3241_CHECK_GL="1"),
)
stdout_lines = []
stderr_lines = []


def collect(stream, lines):
    for line in stream:
        lines.append(line)


threads = [
    threading.Thread(target=collect, args=(process.stdout, stdout_lines)),
    threading.Thread(target=collect, args=(process.stderr, stderr_lines)),
]
for thread in threads:
    thread.start()


def healthy():
    if process.poll() is not None:
        raise RuntimeError(f"Application exited unexpectedly: {process.returncode}")
    if stderr_lines:
        raise RuntimeError("Application emitted diagnostics: " + "".join(stderr_lines))


def wait_for_frame(count):
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        healthy()
        text = "".join(stdout_lines)
        if text.count("Rendering time =") >= count and f"OpenGL frame check passed: scene {count - 1}" in text:
            return
        time.sleep(0.1)
    raise RuntimeError("Rendering/OpenGL frame check did not complete within 60 seconds")


def survive(seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        healthy()
        time.sleep(0.1)


def application_window():
    user32 = ctypes.windll.user32
    found = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]

    @callback_type
    def visit(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == process.pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(visit, 0)
    if len(found) != 1:
        raise RuntimeError(f"Expected one visible GLUT window; found {len(found)}")
    return found[0]


def capture(hwnd, scene):
    from PIL import ImageGrab

    user32 = ctypes.windll.user32
    user32.SetForegroundWindow.argtypes = [wintypes.HWND]
    user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    user32.SetForegroundWindow(hwnd)
    survive(1)
    user32.GetForegroundWindow.restype = wintypes.HWND
    if user32.GetForegroundWindow() != hwnd:
        raise RuntimeError("GLUT window could not be brought to the foreground")
    rect = wintypes.RECT()
    origin = wintypes.POINT(0, 0)
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)) or not user32.ClientToScreen(hwnd, ctypes.byref(origin)):
        raise RuntimeError("Cannot determine GLUT window client coordinates")
    size = (rect.right - rect.left, rect.bottom - rect.top)
    if size != (600, 400):
        raise RuntimeError(f"Unexpected render dimensions: {size}")
    image = ImageGrab.grab(
        bbox=(origin.x, origin.y, origin.x + size[0], origin.y + size[1]),
        all_screens=True,
    ).convert("RGB")
    if max(high for _, high in image.getextrema()) < 20:
        raise RuntimeError("Captured GLUT client area is black; rendering proof failed")
    path = output / f"scene-{scene}.png"
    image.save(path)
    return {"scene": scene, "file": path.name, "size": size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


report = {"platform": platform.platform(), "python_platform": sys.platform,
          "executable": str(executable), "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
          "scenes": [], "passed": False}
try:
    wait_for_frame(1)
    if "CS3241 Lab 5" not in "".join(stdout_lines):
        raise RuntimeError("Startup banner is missing")
    survive(5)
    if output:
        hwnd = application_window()
        report["scenes"].append(capture(hwnd, 0))
        user32 = ctypes.windll.user32
        user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        if not user32.PostMessageW(hwnd, 0x0102, ord("s"), 0):
            raise RuntimeError("Could not send scene-switch input to GLUT")
        wait_for_frame(2)
        if "Rendering Scene 1" not in "".join(stdout_lines):
            raise RuntimeError("Scene switching did not render scene 1")
        report["scenes"].append(capture(hwnd, 1))
    healthy()
    if output:
        if not user32.PostMessageW(hwnd, 0x0102, ord("q"), 0):
            raise RuntimeError("Could not send quit input to GLUT")
        report["exit_code"] = process.wait(timeout=10)
        if report["exit_code"] != 0:
            raise RuntimeError("Application quit with exit code " + str(report["exit_code"]))
    report["passed"] = True
    print("GLUT startup passed; completed frame has no OpenGL errors.")
    if output:
        print("Native Windows screenshots captured for scenes 0 and 1.")
finally:
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    for thread in threads:
        thread.join(timeout=10)
    stdout = "".join(stdout_lines)
    stderr = "".join(stderr_lines)
    print(stdout, end="")
    if stderr:
        print(stderr, file=sys.stderr, end="")
    if output:
        (output / "stdout.log").write_text(stdout, encoding="utf-8")
        (output / "stderr.log").write_text(stderr, encoding="utf-8")
        (output / "runtime-check.json").write_text(json.dumps(report, indent=2) + "\n")
    if stderr:
        raise SystemExit("Application emitted runtime diagnostics")
