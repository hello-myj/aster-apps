'use strict';
// Drive the real Store WASM and the simulated system installer over HTTP.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { Host } = require('../../../aster-sim/src/host');
const { loadPackage } = require('../../../aster-sim/src/package');
const { MemoryFS } = require('../../../aster-sim/src/memory-fs');
const file = path.resolve(process.argv[2] || 'AppStore/build/sim/riscv/dist/riscv/aster.store.ast');
const pkg = loadPackage(file);
const channel = 'https://aster-releases.obs.cn-south-1.myhuaweicloud.com/aster-source.json';
const release = 'https://store-tests.invalid/r1';
const key = 'e'.repeat(64);
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const delay = () => new Promise(r => setTimeout(r, 5));
let mode, blob, sha, width, height, ranges, base;
function packageFor(version, id = 'aster.store') {
  const bytes = fs.readFileSync(file);
  const offset = bytes.readUInt32LE(12), length = bytes.readUInt32LE(16);
  const manifest = JSON.parse(bytes.subarray(offset, offset + length));
  Object.assign(manifest, { id, target_id: 'wasm', version });
  const json = Buffer.from(JSON.stringify(manifest));
  assert(json.length <= length);
  bytes.fill(32, offset, offset + length);
  json.copy(bytes, offset);
  return bytes;
}
const server = http.createServer((req, res) => {
  const send = doc => res.end(JSON.stringify(doc));
  const route = req.url;
  if (route.startsWith('/v1/catalog')) {
    send({ schema: 1, board: 'aster.simulator', target: 'wasm', total: 0, next: -1, items: [] });
  } else if (route === '/channel') {
    send({schema: 1, kind: 'aster-static-source', base_url: release});
  } else if (route === '/r1/setup-aster.simulator-wasm.json') {
    send({schema: 2, board: mode === 'wrong-board' ? 'unknown' : 'aster.simulator',
      target: 'wasm', files: 11, abi_min: 0x40300, features: 3, width, height, release: key});
  } else if (route === '/r1/setup-' + key + '-10.json') {
    send({schema: 1, kind: 'store', index: 10, path: 'store/package.ast', bytes: blob.length,
      sha256: sha, url: 'blob-' + sha});
  } else if (route === '/r1/blob-' + sha) {
    const match = /^bytes=(\d+)-(\d+)$/.exec(req.headers.range);
    assert(match);
    const start = Number(match[1]), end = Number(match[2]);
    ranges.push([start, end]);
    const data = Buffer.from(blob.subarray(start, end + 1));
    if (mode === 'bad-sha' && end >= 4096) data[data.length - 1] ^= 1;
    res.writeHead(206, {'Content-Length': data.length, 'Content-Range': `bytes ${start}-${end}/${blob.length}`});
    res.end(data);
  } else {
    res.writeHead(404).end();
  }
});
function tap(h, x, y) {
  for (const action of [1, 3]) h.input({type:'touch', action, x, y});
}
async function until(h, ready) {
  for (let i = 0; i < 900; i++) {
    h.tick();
    if (ready()) return;
    await delay();
  }
  assert.fail(JSON.stringify(h.logs.slice(-12)));
}
(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  base = 'http://127.0.0.1:' + server.address().port;
  for (mode of ['upgrade', 'portrait', 'current', 'older', 'wrong-board', 'wrong-id', 'bad-sha', 'cancel']) {
    width = mode === 'portrait' ? 240 : 480;
    height = mode === 'portrait' ? 320 : 480;
    ranges = [];
    const current = pkg.manifest.version;
    const [a,b,c] = current.split('.').map(Number);
    const newer = [a,b,c+1].join('.');
    blob = packageFor(mode === 'older' ? '0.0.1' : mode === 'current' ? current : newer,
                      mode === 'wrong-id' ? 'other.game' : 'aster.store');
    sha = digest(blob);
    const running = {...pkg, manifest:{...pkg.manifest, orientation:width < height ? 'portrait' : 'landscape'}};
    const catalog = [running];
    const h = new Host(running, {width, height, network:true, noAudio:true, catalog, memoryStorage:new MemoryFS()});
    // Only redirect transport in the test. Guest sees the production HTTPS
    // channel even though its normal catalog is configured to another URL.
    const original = h.http.text.bind(h.http);
    h.http.text = (...args) => {
      const value = original(...args);
      if (value === channel) return base + '/channel';
      if (value.startsWith(release + '/')) return base + '/r1' + value.slice(release.length);
      return value;
    };
    h.storageFs.writeFileSync(path.join(h.root, 'kv', Buffer.from('store.url').toString('hex')), Buffer.from(base));
    const logged = re => h.logs.some(l => re.test(l.message));
    try {
      h.start();
      await until(h, () => logged(/STORE page/));
      tap(h, 32, height - 30);
      tap(h, width - 45, 14);
      await until(h, () => logged(/STORE (self-update|update check:)/));
      if (mode.startsWith('wrong')) {
        assert(logged(/update check:/));
      } else {
        assert.equal(ranges.length, 1);
        assert.deepEqual(ranges[0], [0, 4095]);
        assert(logged(new RegExp('available=' + (['older','current'].includes(mode) ? 0 : 1))));
        if (['upgrade','portrait','bad-sha'].includes(mode)) {
          tap(h, width - 50, height - 30);
          await until(h, () => h.pendingInstall || logged(/STORE FAIL/));
          if (mode === 'bad-sha') assert(logged(/STORE FAIL/));
          else {
            assert(h.pendingInstall);
            h.stop();
            h.completeInstall();
            assert.equal(catalog[0].manifest.version, newer);
          }
        } else {
          tap(h, 40, height - 30);
          assert(!h.pendingInstall);
          assert.equal(ranges.length, 1);
        }
      }
    } finally { h.stop(); }
  }
  console.log('PASS Store self-update: normal/portrait, newer/equal/older, identity, board, SHA-256 and cancellation');
})().catch(e => { console.error(e); process.exitCode = 1; }).finally(() => server.close());
