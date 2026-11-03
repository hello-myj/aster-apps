"""Build a configured Store without editing the source manifest."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.parse import urlsplit

project = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--sdk", type=Path, default=project.parents[1] / "aster-sdk")
p.add_argument("--target", choices=("riscv", "xtensa"), required=True)
p.add_argument("--server-url", default="http://127.0.0.1:9080")
p.add_argument("--no-aot", action="store_true")
a = p.parse_args()
url = urlsplit(a.server_url)
if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password or url.query or url.fragment or any(ord(c) <= 32 or ord(c) >= 127 or c in "'\\" for c in a.server_url):
    p.error("server URL must be an ASCII HTTP(S) base URL without credentials/query/fragment")
configured = project / "build/configured"
configured.mkdir(parents=True, exist_ok=True)
shutil.copytree(project / "src", configured / "src", dirs_exist_ok=True)
shutil.copytree(project / "assets", configured / "assets", dirs_exist_ok=True)
manifest = (project / "app.toml").read_text(encoding="utf-8-sig")
if a.target == "xtensa":
    manifest = re.sub(r"heap = \d+", "heap = 393216", manifest)
manifest = re.sub(r"defines = \[.*\]", "defines = ['STORE_URL=" + json.dumps(a.server_url.rstrip('/')) + "']", manifest)
(configured / "app.toml").write_text(manifest, encoding="utf-8")
environment = os.environ | {"ASTER_WORKSPACE": str(project)}
cmd = [sys.executable, str(a.sdk.resolve() / "tools/build.py"), str(configured), "--target", a.target, "--no-aot" if a.no_aot else "--aot"]
raise SystemExit(subprocess.run(cmd, env=environment).returncode)
