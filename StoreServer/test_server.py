import json
import struct
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from urllib.parse import urlencode
from http.server import ThreadingHTTPServer
from .server import publish, Store, handler, relative, package_icon


def package(path, target=2, version="1.0.0", icon=None):
    manifest = json.dumps({"id": "test.store.demo", "name": "商城测试", "version": version,
        "abi": {"min": "4.1", "target": "4.2.0"}, "requires": ["display", "touch"]}).encode()
    mo, wo = 64, (64+len(manifest)+3)&~3
    ao = wo+8
    hdr = bytearray(64);hdr[:4] = b"ASTP"
    struct.pack_into("<HHI", hdr, 4, 1, 64, 0)
    struct.pack_into("<9I", hdr, 12, mo, len(manifest), wo, 8, ao, 8, target, 0, 0)
    if icon:
        struct.pack_into("<II", hdr, 40, ao+8, len(icon))
    path.write_bytes(hdr+manifest+b"\0"*(wo-64-len(manifest))+b"\0asm\1\0\0\0"+b"\0aot\1\0\0\0"+(icon or b""))


class ServerTest(unittest.TestCase):
    def test_icons_are_derived_and_hardware_filtered(self):
        import hashlib
        icon = struct.pack("<4sHHHBBI", b"ASTI", 1, 2, 1, 4, 0, 6)+struct.pack("<HH", 0xf800, 0x001f)+bytes([255,0])
        package(self.pkg, version="1.0.1", icon=icon)
        key = publish(self.data,self.pkg,["osptek-p4-st7102"],screens=[[480,480]])
        raw = (self.data/"releases"/key/"release.json").read_bytes()
        with self.get('/v1/catalog') as response:
            item = json.load(response)["items"][0]
        self.assertEqual(item["release"],key)
        with self.get('/v1/releases/'+key+'/icon') as response:
            image = response.read()
            self.assertEqual(response.headers['Content-Type'],'image/bmp')
        self.assertEqual(item['icon'],{'bytes':len(image),'sha256':hashlib.sha256(image).hexdigest()})
        self.assertEqual(image[:2],b'BM')
        # 2:1 artwork is centered vertically; transparent blue becomes white.
        self.assertEqual(image[54+24*144:54+24*144+3],b'\x00\x00\xff')
        self.assertEqual(image[54+24*144+47*3:54+24*144+48*3],b'\xff\xff\xff')
        self.assertEqual((self.data/"releases"/key/"release.json").read_bytes(),raw)
        with self.get('/v1/releases/'+key+'/icon',self.dev|{'board':'other.p4'}) as response:
            self.assertEqual(response.status,409)
        with self.get('/v1/releases/'+self.key+'/icon') as response:
            self.assertEqual(response.status,404)

    def test_invalid_icon_is_optional_and_bounded(self):
        for blob in (b'bad',struct.pack('<4sHHHBBI',b'ASTI',1,65535,1,3,0,2)+b'xx',
                     struct.pack('<4sHHHBBI',b'ASTI',1,2,2,3,0,8)+b'xx'):
            package(self.pkg,icon=blob)
            self.assertIsNone(package_icon(self.pkg))
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory();self.root = Path(self.tmp.name)
        self.data = self.root / "data";self.pkg = self.root / "demo.ast"
        package(self.pkg)
        assets = self.root / "assets";assets.mkdir();(assets / "hello.bin").write_bytes(bytes(range(256))*1000)
        self.key = publish(self.data, self.pkg, ["osptek-p4-st7102"], assets, [[480, 480]])
        self.dev = {"board": "osptek-p4-st7102", "target": "riscv", "abi": 0x040300,
                    "features": 3, "width": 480, "height": 480}
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler(Store(self.data)))
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True);self.thread.start()

    def tearDown(self):
        self.httpd.shutdown();self.httpd.server_close();self.thread.join();self.tmp.cleanup()

    def get(self, path, dev=None, headers=None, method="GET"):
        url = "http://127.0.0.1:%d%s?%s" % (self.httpd.server_port, path, urlencode(self.dev if dev is None else dev))
        request = urllib.request.Request(url, headers=headers or {}, method=method)
        try:
            return urllib.request.urlopen(request)
        except urllib.error.HTTPError as error:
            return error

    def test_catalog_and_capability_matrix(self):
        with self.get("/v1/catalog") as r:
            self.assertEqual(json.load(r)["items"][0]["release"], self.key)
        for key, value in (("board", "other.p4"), ("target", "xtensa"), ("abi", 0x030f00),
                           ("abi", 0x040000), ("features", 1), ("width", 320)):
            dev = self.dev | {key: value}
            with self.get("/v1/catalog", dev) as r:
                self.assertEqual(json.load(r)["items"], [])
            with self.get(f"/v1/releases/{self.key}/files/0", dev) as r:
                self.assertEqual(r.status, 409)

    def test_range_head_and_integrity(self):
        path = f"/v1/releases/{self.key}/files/1"
        with self.get(path, headers={"Range": "bytes=253-259"}) as r:
            self.assertEqual(r.status, 206);self.assertEqual(r.read(), bytes([253,254,255,0,1,2,3]))
            self.assertEqual(r.headers["Content-Range"], "bytes 253-259/256000")
        with self.get(path, method="HEAD") as r:
            self.assertEqual(r.headers["Content-Length"], "256000");self.assertEqual(r.read(), b"")
        with self.get(path, headers={"Range": "bytes=999999-"}) as r:
            self.assertEqual(r.status, 416)
        with self.get(path, headers={"Range": "bytes=1-2", "If-Range": '"old"'}) as r:
            self.assertEqual(r.status, 200);self.assertEqual(len(r.read()), 256000)

    def test_missing_identity_and_traversal(self):
        for path in ("/v1/catalog", "/v1/releases/../server.py", "/releases/anything"):
            with self.get(path, {}) as r:
                self.assertEqual(r.status, 400)
        for name in ("../a", "/etc/passwd", "a\\b", "CON.txt", "a./b", "a:b"):
            with self.assertRaises(ValueError):relative(name)

    def test_release_snapshot_and_immutability(self):
        self.pkg.write_bytes(b"changed after publish")
        with self.get(f"/v1/releases/{self.key}/files/0") as r:
            self.assertTrue(r.read().startswith(b"ASTP"))
        package(self.pkg)
        with self.assertRaises(ValueError):publish(self.data, self.pkg, ["osptek-p4-st7102"])
        payload = self.data / "releases" / self.key / "payload/1"
        payload.write_bytes(b"tamper")
        with self.assertRaises(ValueError):Store(self.data)

    def test_new_release_is_loaded_without_restart(self):
        package(self.pkg, version="1.0.1")
        key = publish(self.data,self.pkg,["osptek-p4-st7102"])
        with self.get("/v1/catalog") as r:
            items=json.load(r)["items"]
            self.assertEqual(len(items),1);self.assertEqual(items[0]["release"],key)


if __name__ == "__main__":unittest.main()
