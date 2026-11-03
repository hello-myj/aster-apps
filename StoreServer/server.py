"""GPL-3.0-or-later. Local HTTP store; immutable releases, bounded streaming.

Python 3.11+, standard library only. No runtime dependency on another repo.
Publishing is an operator CLI operation; the HTTP surface is read-only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import tempfile
import threading
import tomllib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path, PurePosixPath
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent
ID = re.compile(r"[a-zA-Z0-9][a-zA-Z0-9_.-]{0,62}\Z")
DIGEST = re.compile(r"[a-f0-9]{64}\Z")
BACKENDS = {0: "wasm", 1: "xtensa", 2: "riscv", 3: "thumbv7", 4: "thumbv8", 5: "x86_64"}
FEATURES = {"display": 0, "touch": 1, "mouse": 2, "keyboard": 3, "camera": 4,
            "audio-in": 5, "audio-out": 6, "wifi": 7, "bluetooth": 8,
            "accelerometer": 9, "gyroscope": 10, "filesystem": 11,
            "storage.app": 11, "rtc": 12, "psram": 13, "framebuffer": 14,
            "network": 15, "http": 16, "tls": 17, "audio-stream": 18,
            "mqtt": 19, "websocket": 20, "audio-effects": 21,
            "audio-effects-memory": 22}


def encode(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def sha_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def relative(name):
    p = PurePosixPath(name)
    if (not name or len(name.encode()) > 95 or len(p.parts)>16 or p.is_absolute() or
            any(x in ("", ".", "..") for x in name.split("/")) or
            any(ord(c) < 32 or c in '\\:"<>|?*' for c in name) or
            any(part.endswith((".", " ")) or re.match(r"(?i)^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)", part)
                for part in p.parts)):
        raise ValueError("invalid resource path: " + name)
    return name


def version(text):
    if not isinstance(text, str) or not re.fullmatch(r"\d+\.\d+(?:\.\d+)?", text):
        raise ValueError("invalid numeric version")
    parts = tuple(map(int, text.split(".")))
    if any(n > 255 for n in parts):
        raise ValueError("version component out of range")
    return parts + (0,) * (3-len(parts))


def ast_metadata(path):
    """Read bounded metadata without loading code/resources into memory."""
    size = path.stat().st_size
    with path.open("rb") as stream:
        hdr = stream.read(72)
        if len(hdr) < 64 or hdr[:4] != b"ASTP":
            raise ValueError("not an ASTP package")
        fmt, hs, flags = struct.unpack_from("<HHI", hdr, 4)
        if fmt != 1 or hs not in (64, 72) or flags & ~5:
            raise ValueError("unsupported ASTP header")
        mo, ml, wo, wl, ao, al, target, io, il = struct.unpack_from("<9I", hdr, 12)
        if not ml or ml > 65536 or not wl or target not in BACKENDS or bool(al) != bool(target):
            raise ValueError("release requires a bounded manifest and target AOT")
        spans = [(mo, ml), (wo, wl), (ao, al), (io, il)]
        if hs == 72:
            spans.append(struct.unpack_from("<II", hdr, 64))
        used = sorted((off, off+n) for off, n in spans if n)
        if any(a < hs or b > size for a, b in used) or any(
                used[i][1] > used[i+1][0] for i in range(len(used)-1)):
            raise ValueError("invalid/overlapping package sections")
        stream.seek(mo)
        manifest = json.loads(stream.read(ml))
    if not ID.fullmatch(manifest.get("id", "")) or ".." in manifest["id"]:
        raise ValueError("invalid APP identifier")
    abi = manifest.get("abi", {})
    if not isinstance(abi, dict):
        raise ValueError("package ABI descriptor missing")
    minimum = version(abi.get("min", ""))
    required = manifest.get("requires", [])
    if not isinstance(required, list) or any(x not in FEATURES for x in required):
        raise ValueError("unsupported capability requirement")
    return {"id": manifest["id"], "name": manifest.get("name", manifest["id"]),
            "version": manifest["version"], "abi_min": list(minimum),
            "abi_target": list(version(abi.get("target", ""))),
            "target": BACKENDS[target], "requires": required,
            "permissions": manifest.get("permissions", []),
            "description": manifest.get("description", ""),
            "category": manifest.get("category", "tool")}


def publish(data, package, boards, resources=None, screens=None):
    """Freeze one target build and its independent resource tree atomically."""
    package = package.resolve(strict=True)
    info = ast_metadata(package)
    if not boards or any(not ID.fullmatch(b) for b in boards):
        raise ValueError("an explicit nonempty board allowlist is required")
    info.update(schema=1, boards=sorted(set(boards)), screens=screens or [], files=[])
    releases = data / "releases"
    releases.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="publish-", dir=data) as tmp:
        staging = Path(tmp)
        payload = staging / "payload"
        payload.mkdir()
        sources = [("package", "package.ast", package)]
        if resources:
            root = resources.resolve(strict=True)
            if not root.is_dir():
                raise ValueError("resources must be a directory")
            for p in sorted(root.rglob("*")):
                if p.is_symlink():
                    raise ValueError("resource symlinks are not accepted")
                if p.is_file():
                    sources.append(("resource", relative(p.relative_to(root).as_posix()), p))
        if len(sources) > 4097:
            raise ValueError("maximum 4096 resource files")
        names = set()
        for i, (role, name, source) in enumerate(sources):
            folded = role + ":" + name.casefold()
            if folded in names:
                raise ValueError("resource paths collide on FAT/Windows")
            names.add(folded)
            dest = payload / str(i)
            shutil.copyfile(source, dest)
            info["files"].append({"index": i, "role": role, "path": name,
                                  "bytes": dest.stat().st_size, "sha256": sha_file(dest)})
        info["total_bytes"] = sum(f["bytes"] for f in info["files"])
        if sum(f["bytes"] for f in info["files"] if f["role"] == "resource") > 1073741824:
            raise ValueError("resource tree exceeds 1 GiB")
        identity = hashlib.sha256(encode(info)).hexdigest()
        for existing in releases.glob("*/release.json"):
            old = json.loads(existing.read_bytes())
            if (old["id"] == info["id"] and old["version"] == info["version"] and
                old["target"] == info["target"] and set(old["boards"]) & set(info["boards"]) and
                existing.parent.name != identity):
                raise ValueError("published versions are immutable; use a new APP version")
        (staging / "release.json").write_bytes(encode(info))
        dest = releases / identity
        if not dest.exists():
            os.rename(staging, dest)
            # TemporaryDirectory tolerates the renamed staging root.
        return identity


def device(query):
    def one(key):
        values = query.get(key, [])
        if len(values) != 1:
            raise ValueError("missing/duplicate device field: " + key)
        return values[0]
    board = one("board")
    if not ID.fullmatch(board):
        raise ValueError("invalid board")
    result = {"board": board, "target": one("target")}
    if result["target"] not in BACKENDS.values():
        raise ValueError("invalid target")
    for key, limit in (("abi", 0xffffff), ("features", (1 << 63)-1),
                       ("width", 4096), ("height", 4096)):
        raw = one(key)
        if not re.fullmatch(r"[0-9]{1,19}", raw):
            raise ValueError("invalid device number")
        result[key] = int(raw)
        if result[key] > limit:
            raise ValueError("device number out of range")
    return result


def incompatibility(info, dev):
    reasons = []
    abi = dev["abi"]
    current = [abi >> 16, (abi >> 8) & 255, abi & 255]
    if dev["board"] not in info["boards"]:
        reasons.append("board")
    if dev["target"] != info["target"]:
        reasons.append("aot_target")
    if current[0] != info["abi_min"][0] or current < info["abi_min"]:
        reasons.append("abi")
    if info["screens"] and [dev["width"], dev["height"]] not in info["screens"]:
        reasons.append("screen")
    if any(not dev["features"] & (1 << FEATURES[f]) for f in info["requires"]):
        reasons.append("capability")
    return reasons


class Store:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.releases = {}
        self.lock = threading.RLock()
        self.refresh()

    def refresh(self):
        with self.lock:
            self._refresh()

    def _refresh(self):
        for path in sorted((self.root / "releases").glob("*/release.json")):
            key = path.parent.name
            if key in self.releases:
                continue
            if path.is_symlink() or path.parent.is_symlink():
                raise ValueError("release symlinks are not accepted")
            raw = path.read_bytes()
            if not DIGEST.fullmatch(key) or hashlib.sha256(raw).hexdigest() != key:
                raise ValueError("release metadata integrity failure: " + str(path))
            info = json.loads(raw)
            for f in info["files"]:
                p = path.parent / "payload" / str(f["index"])
                if p.is_symlink() or p.stat().st_size != f["bytes"] or sha_file(p) != f["sha256"]:
                    raise ValueError("release payload integrity failure")
            self.releases[key] = info

    def catalog(self, dev):
        self.refresh()
        # One newest compatible release per APP, selected independently for
        # this exact board. Unknown/incompatible releases never get file URLs.
        newest = {}
        for key, info in list(self.releases.items()):
            if incompatibility(info, dev):
                continue
            old = newest.get(info["id"])
            if old is None or version(info["version"]) > version(old[1]["version"]):
                newest[info["id"]] = (key, info)
        return [{"release": key, "id": info["id"], "name": info["name"],
                 "version": info["version"], "description": info["description"],
                 "bytes": info["total_bytes"], "file_count": len(info["files"]),
                 "permissions": info["permissions"]}
                for key, info in sorted(newest.values(), key=lambda x: x[1]["id"])]


def handler(store):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        server_version = "AsterStore/1"
        sys_version = ""

        def setup(self):
            super().setup()
            self.connection.settimeout(20)

        def reply(self, code, data, content_type="application/json; charset=utf-8", extra=None):
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Connection", "close")
            for k, v in (extra or {}).items():
                self.send_header(k, str(v))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(data)
            self.close_connection = True

        def do_HEAD(self):
            self.do_GET()

        def do_GET(self):
            try:
                if len(self.path) > 2048:
                    raise ValueError("request URL too long")
                url = urlsplit(self.path)
                if url.path == "/healthz":
                    return self.reply(200, encode({"ok": True, "schema": 1}))
                query = parse_qs(url.query, keep_blank_values=True, max_num_fields=12)
                dev = device(query)
                page = int(query.get("page", ["0"])[0])
                if page < 0 or page > 10000:
                    raise ValueError("invalid page")
                if url.path == "/v1/catalog":
                    items = store.catalog(dev)
                    return self.reply(200, encode({"schema": 1, "total": len(items),
                        "items": items[page*8:(page+1)*8], "next": page+1 if (page+1)*8 < len(items) else -1}))
                match = re.fullmatch(r"/v1/releases/([a-f0-9]{64})(?:/files/(\d+))?", url.path)
                if not match or match[1] not in store.releases:
                    return self.reply(404, encode({"error": "not_found"}))
                key, index = match.groups()
                info = store.releases[key]
                reasons = incompatibility(info, dev)
                if reasons:
                    return self.reply(409, encode({"error": "incompatible", "reasons": reasons}))
                if index is None:
                    files = info["files"]
                    return self.reply(200, encode({"schema": 1, "release": key,
                        "id": info["id"], "version": info["version"], "boards": info["boards"],
                        "target": info["target"], "abi_min": info["abi_min"],
                        "requires": info["requires"], "screens": info["screens"],
                        "files": files[page*8:(page+1)*8], "total": len(files),
                        "next": page+1 if (page+1)*8 < len(files) else -1}))
                number = int(index)
                if number >= len(info["files"]):
                    return self.reply(404, encode({"error": "not_found"}))
                self.send_file(key, info["files"][number])
            except (ValueError, KeyError, TypeError, OverflowError):
                self.reply(400, encode({"error": "invalid_request"}))
            except (BrokenPipeError, ConnectionResetError, TimeoutError):
                self.close_connection = True

        def send_file(self, key, item):
            path = store.root / "releases" / key / "payload" / str(item["index"])
            length = item["bytes"]
            etag = '"' + item["sha256"] + '"'
            start, end, code = 0, length-1, 200
            range_header = self.headers.get("Range")
            if range_header:
                match = re.fullmatch(r"bytes=(\d+)-(\d*)", range_header)
                if not match:
                    return self.reply(416, b"", extra={"Content-Range": f"bytes */{length}"})
                start = int(match[1]); end = min(int(match[2]) if match[2] else end, end)
                if start > end:
                    return self.reply(416, b"", extra={"Content-Range": f"bytes */{length}"})
                if self.headers.get("If-Range", etag) != etag:
                    start, end = 0, length-1
                else:
                    code = 206
            self.send_response(code)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(end-start+1))
            self.send_header("ETag", etag)
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Cache-Control", "private, max-age=31536000, immutable")
            self.send_header("Connection", "close")
            if code == 206:
                self.send_header("Content-Range", f"bytes {start}-{end}/{length}")
            self.end_headers()
            self.close_connection = True
            if self.command == "HEAD":
                return
            with path.open("rb") as stream:
                stream.seek(start)
                remaining = end-start+1
                while remaining:
                    chunk = stream.read(min(65536, remaining))
                    if not chunk:
                        break
                    self.wfile.write(chunk); remaining -= len(chunk)

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=ROOT / "data")
    commands = parser.add_subparsers(dest="command", required=True)
    pub = commands.add_parser("publish")
    pub.add_argument("package", type=Path)
    pub.add_argument("--board", action="append", required=True)
    pub.add_argument("--resources", type=Path)
    pub.add_argument("--screen", action="append", help="exact WIDTHxHEIGHT, repeatable")
    proj = commands.add_parser("publish-project")
    proj.add_argument("project", type=Path)
    proj.add_argument("--target", required=True)
    serve = commands.add_parser("serve")
    serve.add_argument("--host", default="127.0.0.1")
    serve.add_argument("--port", type=int, default=9080)
    args = parser.parse_args()
    if args.command == "publish-project":
        project = args.project.resolve(strict=True)
        config = tomllib.loads((project / "store.toml").read_text(encoding="utf-8-sig"))
        candidates = [v for v in config["variants"] if v["target"] == args.target]
        if len(candidates) != 1:
            parser.error("project must declare exactly one variant for this target")
        v = candidates[0]
        package = (project / v["package"]).resolve(strict=True)
        resources = (project / v["resources"]).resolve(strict=True) if v.get("resources") else None
        if not package.is_relative_to(project) or (resources and not resources.is_relative_to(project)):
            parser.error("project release paths must stay within the project")
        info = ast_metadata(package)
        if info["id"] != config["id"] or info["target"] != args.target:
            parser.error("project identity/target does not match compiled package")
        print(publish(args.data, package, v["boards"], resources, v.get("screens")))
    elif args.command == "publish":
        screens = []
        for item in args.screen or []:
            if not re.fullmatch(r"[1-9]\d{0,3}x[1-9]\d{0,3}", item):
                parser.error("screen must be WIDTHxHEIGHT")
            screens.append(list(map(int, item.split("x"))))
        print(publish(args.data, args.package, args.board, args.resources, screens))
    else:
        store = Store(args.data)
        httpd = ThreadingHTTPServer((args.host, args.port), handler(store))
        httpd.daemon_threads = True
        print(f"Aster Store: http://{args.host}:{httpd.server_port}, {len(store.releases)} releases", flush=True)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            httpd.server_close()


if __name__ == "__main__":
    main()
