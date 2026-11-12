"""Verify the interactive GLUT application starts and survives initialization."""

import subprocess
import sys
from pathlib import Path


executable = Path(sys.argv[1]).resolve()
process = subprocess.Popen(
    [str(executable)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
)
try:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        stdout, stderr = process.communicate(timeout=10)
        if "CS3241 Lab 5" not in stdout:
            raise SystemExit(f"Application did not print its startup banner.\n{stdout}\n{stderr}")
        if stderr.strip():
            raise SystemExit(f"Application emitted startup diagnostics:\n{stderr}")
        print("GLUT startup passed: application remained running for 5 seconds.")
    else:
        raise SystemExit(
            f"Application exited during startup ({process.returncode}).\n{stdout}\n{stderr}"
        )
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
