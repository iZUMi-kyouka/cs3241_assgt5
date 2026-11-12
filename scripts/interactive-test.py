"""Exercise the real GLUT window on native Windows or Linux with a window manager."""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

from PIL import ImageChops, ImageGrab


class Desktop:
    def __init__(self, pid):
        self.windows = sys.platform == "win32"
        if self.windows:
            self.api = ctypes.windll.user32
            self.api.SetProcessDPIAware()
            callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
            self.api.EnumWindows.argtypes = [callback, wintypes.LPARAM]
            self.api.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
            self.api.IsWindowVisible.argtypes = [wintypes.HWND]
            found = []

            @callback
            def visit(hwnd, _):
                owner = wintypes.DWORD()
                self.api.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
                if owner.value == pid and self.api.IsWindowVisible(hwnd):
                    found.append(hwnd)
                return True

            self.api.EnumWindows(visit, 0)
            if len(found) != 1:
                raise RuntimeError(f"Expected one GLUT window, found {found}")
            self.window = found[0]
            self.api.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
            self.api.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
            self.api.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
            self.api.SetForegroundWindow.argtypes = [wintypes.HWND]
            self.api.GetForegroundWindow.restype = wintypes.HWND
            self.api.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
            self.api.GetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int]
            self.api.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int,
                                            ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
            self.api.SetForegroundWindow(self.window)
            if self.api.GetForegroundWindow() != self.window:
                raise RuntimeError("Cannot focus the GLUT window")
        else:
            self.window = self.xd("search", "--onlyvisible", "--pid", pid,
                                  "--name", "CS3241 Lab 5: Ray Tracing").splitlines()[0]
            self.xd("windowfocus", "--sync", self.window)

    @staticmethod
    def xd(*args):
        return subprocess.check_output(["xdotool", *map(str, args)], text=True).strip()

    def post(self, message, wparam=0, lparam=0):
        if not self.api.PostMessageW(self.window, message, wparam, lparam):
            raise RuntimeError(f"Cannot deliver native window message {message:#x}")

    def geometry(self):
        if self.windows:
            rect, origin = wintypes.RECT(), wintypes.POINT(0, 0)
            if not self.api.GetClientRect(self.window, ctypes.byref(rect)):
                raise RuntimeError("Cannot query client rectangle")
            if not self.api.ClientToScreen(self.window, ctypes.byref(origin)):
                raise RuntimeError("Cannot query client origin")
            return origin.x, origin.y, rect.right - rect.left, rect.bottom - rect.top
        # xdotool geometry can include frame offsets under a window manager.
        info = subprocess.check_output(["xwininfo", "-id", self.window], text=True)
        fields = [r"Absolute upper-left X:\s*(-?\d+)", r"Absolute upper-left Y:\s*(-?\d+)",
                  r"Width:\s*(\d+)", r"Height:\s*(\d+)"]
        return tuple(int(re.search(pattern, info).group(1)) for pattern in fields)

    def title(self):
        if not self.windows:
            return self.xd("getwindowname", self.window)
        text = ctypes.create_unicode_buffer(512)
        self.api.GetWindowTextW(self.window, text, len(text))
        return text.value

    def key(self, name, down=None):
        if not self.windows:
            self.xd("key" if down is None else "keydown" if down else "keyup", name)
            return
        codes = {"Up": 0x26, "Down": 0x28, "Left": 0x25, "Right": 0x27,
                 "Home": 0x24, "F11": 0x7A}
        if name not in codes:
            self.post(0x0102, ord(" " if name == "space" else name))  # WM_CHAR
        elif down is None:
            self.key(name, True)
            self.key(name, False)
        else:
            self.post(0x0100 if down else 0x0101, codes[name], 1 if down else 0xC0000001)

    def mouse(self, action, x=300, y=200):
        if not self.windows:
            if action == "move":
                self.xd("mousemove", "--window", self.window, x, y)
            else:
                self.xd({"down": "mousedown", "up": "mouseup", "wheel": "click"}[action],
                        4 if action == "wheel" else 1)
            return
        point = (x & 0xFFFF) | ((y & 0xFFFF) << 16)
        if action == "wheel":
            ox, oy, _, _ = self.geometry()
            self.post(0x020A, 120 << 16, ((ox + x) & 0xFFFF) | (((oy + y) & 0xFFFF) << 16))
        else:
            message = {"move": 0x0200, "down": 0x0201, "up": 0x0202}[action]
            held = action == "down" or (action == "move" and getattr(self, "dragging", False))
            self.post(message, 1 if held else 0, point)
            if action in ("down", "up"):
                self.dragging = action == "down"

    def resize(self, width, height):
        if not self.windows:
            self.xd("windowsize", "--sync", self.window, width, height)
            return
        rect = wintypes.RECT(0, 0, width, height)
        style = self.api.GetWindowLongW(self.window, -16)
        extended = self.api.GetWindowLongW(self.window, -20)
        if not self.api.AdjustWindowRectEx(ctypes.byref(rect), style, False, extended):
            raise RuntimeError("Cannot calculate window frame size")
        if not self.api.SetWindowPos(self.window, None, 40, 40,
                                     rect.right - rect.left, rect.bottom - rect.top, 0x0004):
            raise RuntimeError("Cannot resize native window")

    def screen_size(self):
        if self.windows:
            return self.api.GetSystemMetrics(0), self.api.GetSystemMetrics(1)
        return tuple(map(int, self.xd("getdisplaygeometry").split()))

    def capture(self):
        x, y, width, height = self.geometry()
        screen_width, screen_height = self.screen_size()
        if x < 0 or y < 0 or x + width > screen_width or y + height > screen_height:
            raise RuntimeError(f"Client lies outside desktop: {(x, y, width, height)}")
        if self.windows:
            if self.api.GetForegroundWindow() != self.window:
                raise RuntimeError("Another window covers the GLUT window")
            return ImageGrab.grab(bbox=(x, y, x + width, y + height), all_screens=True).convert("RGB")
        return ImageGrab.grab(xdisplay=os.environ["DISPLAY"]).crop((x, y, x + width, y + height)).convert("RGB")


def changed_fraction(a, b):
    difference = ImageChops.difference(a, b)
    channels = difference.split()
    maximum = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
    return 1.0 - maximum.histogram()[0] / (a.width * a.height)


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(executable, output):
    output.mkdir(parents=True, exist_ok=True)
    stdout_path, stderr_path = output / "stdout.log", output / "stderr.log"
    report = {"passed": False, "platform": sys.platform, "checks": [], "scales": [],
              "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
    with stdout_path.open("w") as stdout, stderr_path.open("w") as stderr:
        process = subprocess.Popen([str(executable)], stdout=stdout, stderr=stderr,
                                   env=dict(os.environ, CS3241_CHECK_GL="1"))

        def healthy():
            check(process.poll() is None, f"Application exited: {process.returncode}")
            check(not stderr_path.read_text().strip(), "Runtime diagnostics: " + stderr_path.read_text())

        def frames():
            return stdout_path.read_text().count("OpenGL frame check passed")

        def wait_frame(previous):
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                healthy()
                if frames() > previous:
                    return
                time.sleep(0.05)
            raise RuntimeError("No completed error-free frame within 30 seconds")

        def shot(name):
            time.sleep(0.4)
            healthy()
            image = desktop.capture()
            image.save(output / (name + ".png"))
            return image

        def key(name):
            before = frames()
            desktop.key(name)
            wait_frame(before)

        def wait_motion(reference, name):
            deadline = time.monotonic() + 30
            previous = frames()
            while time.monotonic() < deadline:
                wait_frame(previous)
                image = shot(name)
                if changed_fraction(reference, image) > 0.001:
                    return
                previous = frames()
            raise RuntimeError("Completed frames showed no orbital motion")

        def wait_idle():
            # v120 Debug can take over a second per frame on the software GL
            # runner. Wait for pending rendering to finish rather than assuming
            # a fixed sub-second delay means a pause key has been handled.
            timings = re.findall(r"Rendering time = (\d+)ms", stdout_path.read_text())
            quiet_seconds = max(0.5, int(timings[-1]) / 1000 * 2 + 0.5)
            deadline = time.monotonic() + 30
            previous, last_change = frames(), time.monotonic()
            while time.monotonic() < deadline:
                healthy()
                current = frames()
                if current != previous:
                    previous, last_change = current, time.monotonic()
                if time.monotonic() - last_change >= quiet_seconds:
                    return
                time.sleep(0.05)
            raise RuntimeError("Animation did not become idle after pause")

        try:
            wait_frame(0)
            desktop = Desktop(process.pid)
            desktop.mouse("move")
            original = shot("original")
            before = frames()
            desktop.key("Up", True)
            time.sleep(0.5)
            desktop.key("Up", False)
            wait_frame(before)
            moved = shot("moved")
            check(changed_fraction(original, moved) > 0.01, "Arrow movement had no visible effect")
            time.sleep(0.5)
            check(changed_fraction(moved, shot("key-released")) == 0, "Arrow movement stuck after release")
            key("Home")
            check(changed_fraction(original, shot("reset")) == 0, "Camera reset changed the original image")
            report["checks"].extend(["arrow movement", "key release stops movement", "camera reset"])

            desktop.mouse("move")
            before = frames()
            desktop.mouse("down")
            desktop.mouse("move", 370, 170)
            time.sleep(0.4)
            desktop.mouse("up", 370, 170)
            wait_frame(before)
            looked = shot("looked")
            check(changed_fraction(original, looked) > 0.01, "Mouse drag did not rotate camera")
            desktop.mouse("move", 450, 240)
            check(changed_fraction(looked, shot("mouse-released")) == 0, "Passive mouse motion rotated camera")
            key("Home")
            before = frames()
            desktop.mouse("wheel")
            wait_frame(before)
            check(changed_fraction(original, shot("zoomed")) > 0.01, "Scroll wheel did not zoom")
            report["checks"].extend(["drag rotation", "mouse release stops rotation", "scroll zoom"])

            key("s")
            solar = shot("solar")
            check(changed_fraction(original, solar) > 0.8, "S did not switch scenes")
            wait_motion(solar, "solar-animated")
            desktop.key("space")
            wait_idle()
            paused = shot("solar-paused")
            time.sleep(0.5)
            check(changed_fraction(paused, shot("solar-still")) == 0, "Pause did not stop planets")
            desktop.key("space")
            wait_motion(paused, "solar-resumed")
            desktop.key("space")
            wait_idle()
            report["checks"].extend(["S scene switch", "orbital animation", "pause", "resume"])

            before = frames()
            desktop.resize(883, 507)
            wait_frame(before)
            for index, scale in enumerate([1.0, 0.9, 0.7, 0.5, 0.3, 1.0]):
                if index:
                    key("r")
                image = shot(f"scale-{int(scale * 100)}")
                check(image.size == (883, 507), f"Wrong client size: {image.size}")
                check(image.getbbox() == (0, 0, *image.size),
                      f"Image has black border/letterboxing at {scale}x: bbox={image.getbbox()}, size={image.size}")
                red, green, blue = image.split()
                masks = [red.point(lambda x: 255 if x > 250 else 0),
                         green.point(lambda x: 255 if 190 < x < 225 else 0),
                         blue.point(lambda x: 255 if x < 75 else 0)]
                sun = ImageChops.multiply(ImageChops.multiply(masks[0], masks[1]), masks[2]).getbbox()
                check(sun is not None, "Sun missing in capture")
                ratio = (sun[2] - sun[0]) / (sun[3] - sun[1])
                check(0.9 < ratio < 1.1, f"Sphere distorted at scale {scale}: {ratio}")
                title = desktop.title()
                expected = f"{int(scale * 100 + .5)}% ({int(883 * scale + .5)}x{int(507 * scale + .5)})"
                check(expected in title, f"Wrong R cycle/internal size: {title}; expected {expected}")
                report["scales"].append({"multiplier": scale, "title": title, "sun_aspect": ratio})
            report["checks"].extend(["window resize", "all five R scales and wraparound", "aspect ratio", "no letterboxing"])

            for _ in range(4):
                key("r")
            key("F11")
            full = shot("fullscreen")
            check(full.size == desktop.screen_size(), "Fullscreen does not match desktop size")
            check(full.getbbox() == (0, 0, *full.size), "Fullscreen has letterboxing")
            expected = f"30% ({int(full.width * .3 + .5)}x{int(full.height * .3 + .5)})"
            check(expected in desktop.title(), "Fullscreen lost internal multiplier")
            key("F11")
            check(desktop.geometry()[2:] == (883, 507), "Leaving fullscreen did not restore client size")
            title = desktop.title()
            before = frames()
            desktop.key("Right", True)
            time.sleep(0.4)
            desktop.key("Right", False)
            wait_frame(before)
            check(desktop.title() == title, "Moving automatically changed resolution")
            report["checks"].extend(["fullscreen fills desktop", "fullscreen restores window", "no automatic quality drop"])

            desktop.key("q")
            report["exit_code"] = process.wait(timeout=10)
            check(process.returncode == 0, f"Quit failed: {process.returncode}")
            check(not stderr_path.read_text().strip(), "Runtime diagnostics at exit")
            report["passed"] = True
            print(json.dumps(report, indent=2))
        except Exception as error:
            report["error"] = str(error)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            (output / "interactive-check.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output)
