const assert = require('assert');
const { installMusic } = require('../web/music-installer.js');

(async () => {
  const calls = [];
  const bytes = Buffer.from(Array.from({ length: 2500 }, (_, i) => i % 256));
  bytes.write('MGMP');
  const request = async (path, data) => {
    calls.push([path, data]);
    if (path === '/modules/music') return { status: 200, ok: true, json: async () => ({ installed: false, version: 1 }) };
    if (path.endsWith('/start')) return { ok: true, json: async () => ({ token: 42 }) };
    return { ok: true, text: async () => 'OK' };
  };
  await installMusic({ request, loadPackage: async () => bytes.toString('base64'), progress: () => {} });
  assert.deepStrictEqual(calls.map(x => x[0]), ['/modules/music', '/modules/music/start', '/modules/music/chunk', '/modules/music/chunk', '/modules/music/chunk', '/modules/music/finish']);
  assert.deepStrictEqual(calls.filter(x => x[0].endsWith('/chunk')).map(x => x[1].offset), [0, 1024, 2048]);
  assert.strictEqual(calls.at(-1)[1].token, 42);
  let loaded = false;
  await installMusic({ request: async () => ({ status: 404, ok: false }), loadPackage: async () => { loaded = true; } });
  assert.strictEqual(loaded, false, 'older firmware must continue using its built-in Music');
  const checksum = bytes.readUInt32LE(12);
  const sameCalls=[];
  await installMusic({ request: async path => { sameCalls.push(path); return { status:200,ok:true,json:async()=>({installed:true,version:1,checksum}) }; }, loadPackage: async()=>bytes.toString('base64') });
  assert.deepStrictEqual(sameCalls, ['/modules/music'], 'unchanged packages must not be rewritten');
  await assert.rejects(() => installMusic({
    request: async path => path === '/modules/music' ? { status: 200, ok: true, json: async () => ({ installed: false, version: 1 }) } : { ok: false, text: async () => 'Release the winch before installing' },
    loadPackage: async () => bytes.toString('base64')
  }), /Release the winch/);
  console.log('Music installer tests passed');
})().catch(error => { console.error(error); process.exit(1); });
