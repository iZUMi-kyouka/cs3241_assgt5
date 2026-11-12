"""Verify the assignment's original Windows configuration and bundled dependencies."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
manifest = json.loads((ROOT / "scripts/original-files.sha256.json").read_text())
for name, expected in manifest.items():
    path = ROOT / name
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise SystemExit("Original assignment file changed or missing: " + name)
print("Verified: original solution, project, and bundled dependencies are unchanged.")
