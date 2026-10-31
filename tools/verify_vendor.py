"""Verify the exact, licensed SMAA source/data imported into this repository."""
import hashlib
import json
from pathlib import Path

for family in ("smaa", "fxaa"):
    root = Path(__file__).resolve().parents[1] / "third_party" / family
    manifest = json.loads((root / "UPSTREAM.json").read_text(encoding="utf-8"))
    for name, expected in manifest["files"].items():
        path = (root / name).resolve()
        if not path.is_relative_to(root.resolve()):
            raise SystemExit("Vendor manifest path escapes its directory")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"Pinned {family} file changed: {name}")
    print(f"Verified {len(manifest['files'])} {family} files at {manifest['commit']}; source/license notices retained")
