// Browsers can't open raw TCP sockets, so this tiny bridge (no dependencies)
// translates HTTP calls from the web UI into your TCP protocol.
// Usage: ./strt   (your API)   then   node bridge.js   ->  http://localhost:3000
const http = require('http'), net = require('net'), fs = require('fs'), path = require('path');
const HOST = process.env.TCP_HOST || '127.0.0.1';
const CPORT = +process.env.TCP_PORT || 8012;
const PORT = +process.env.PORT || 3000;
const MIME = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript', '.mp4': 'video/mp4',
  '.webm': 'video/webm', '.mkv': 'video/x-matroska', '.mp3': 'audio/mpeg', '.wav': 'audio/wav' };

// Line reader over a socket (handles several lines arriving in one chunk)
function lineReader(sock) {
  let buf = '', err = null; const q = [];
  sock.setEncoding('utf8');
  const flush = () => {
    while (q.length) {
      const i = buf.indexOf('\n');
      if (i >= 0) { q.shift().res(buf.slice(0, i).trim()); buf = buf.slice(i + 1); }
      else if (err) q.shift().rej(err);
      else break;
    }
  };
  sock.on('data', d => { buf += d; flush(); });
  sock.on('error', e => { err = e; flush(); });
  sock.on('close', () => { err = err || new Error('Server closed connection'); flush(); });
  return () => new Promise((res, rej) => { q.push({ res, rej }); flush(); });
}

const clean = s => { if (typeof s !== 'string' || !s || /[\r\n]/.test(s)) throw new Error('Invalid input'); return s; };
const word = s => { clean(s); if (/\s/.test(s)) throw new Error('Spaces are not allowed here'); return s; };

async function open(user, pass) {
  const sock = net.connect(CPORT, HOST);
  sock.setTimeout(20000, () => sock.destroy());
  await new Promise((res, rej) => { sock.once('connect', res); sock.once('error', rej); });
  const read = lineReader(sock);
  sock.write(clean(user) + '\n' + clean(pass) + '\n');
  if ((await read()) !== 'LOGIN_OK') { sock.destroy(); const e = new Error('Login failed'); e.status = 401; throw e; }
  return { sock, read };
}

const body = req => new Promise((res, rej) => {
  let s = ''; req.on('data', d => s += d); req.on('end', () => { try { res(JSON.parse(s || '{}')); } catch (e) { rej(e); } });
});
const json = (res, code, obj) => { res.writeHead(code, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(obj)); };

const COMMANDS = {
  ADD_USER: a => `ADD_USER ${word(a[0])} ${word(a[1])}`,
  UPDATE_USER: a => `UPDATE_USER ${word(a[0])} ${word(a[1])}`,
  DELETE_USER: a => `DELETE_USER ${word(a[0])}`,
  DELETE: a => `DELETE ${clean(a[0])}`,
};

async function handle(req, res) {
  const url = new URL(req.url, 'http://x');

  if (url.pathname === '/api/login' && req.method === 'POST') {
    const b = await body(req); (await open(b.username, b.password)).sock.end();
    return json(res, 200, { ok: true });
  }

  if (url.pathname === '/api/command' && req.method === 'POST') {
    const b = await body(req);
    if (!COMMANDS[b.action]) throw new Error('Unknown action');
    const { sock, read } = await open(b.username, b.password);
    sock.write(COMMANDS[b.action](b.args || []) + '\n');
    const reply = await read(); sock.end();
    return json(res, 200, { reply });
  }

  if (url.pathname === '/api/put' && req.method === 'POST') {
    const user = decodeURIComponent(req.headers['x-user'] || ''), pass = decodeURIComponent(req.headers['x-pass'] || '');
    const { sock, read } = await open(user, pass);
    sock.write(`PUT ${word(url.searchParams.get('recipient'))} ${word(url.searchParams.get('filename'))}\n`);
    const m = /^TRANSFER_PORT (\d+)/.exec(await read());
    if (!m) { sock.destroy(); throw new Error('Server refused upload'); }
    const data = net.connect(+m[1], HOST);
    data.on('connect', () => req.pipe(data));
    data.on('error', e => json(res, 502, { error: e.message }));
    data.on('close', () => { sock.end(); if (!res.headersSent) json(res, 200, { reply: 'UPLOAD_COMPLETE' }); });
    return;
  }

  if (url.pathname === '/api/stream' && req.method === 'POST') {
    const b = await body(req);
    const { sock, read } = await open(b.username, b.password);
    sock.write(`SEARCH_GENRE ${clean(b.genre)}\n`);
    let file = null, port = null;
    for (let i = 0; i < 2 && !port; i++) {
      const l = await read();
      if (l.startsWith('FILE_NOT_FOUND')) { sock.end(); return json(res, 404, { error: 'No matching media found' }); }
      if (l.startsWith('FILE_FOUND')) file = l.slice(10).trim();
      const m = /^TRANSFER_PORT (\d+)/.exec(l); if (m) port = +m[1];
    }
    if (!port) { sock.destroy(); throw new Error('No transfer port received'); }
    const data = net.connect(port, HOST);
    data.on('error', () => res.destroy());
    data.on('connect', () => {
      res.writeHead(200, { 'Content-Type': MIME[path.extname(file || '').toLowerCase()] || 'application/octet-stream',
        'X-File-Path': encodeURIComponent(file || '') });
      data.pipe(res);
    });
    data.on('close', () => sock.end());
    return;
  }

  // static files
  let p = path.join(__dirname, url.pathname === '/' ? 'index.html' : url.pathname);
  if (!p.startsWith(__dirname) || p === __filename) { res.writeHead(403); return res.end(); }
  fs.readFile(p, (e, d) => {
    if (e) { res.writeHead(404); return res.end('Not found'); }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(p)] || 'application/octet-stream' }); res.end(d);
  });
}

http.createServer((req, res) => handle(req, res).catch(e => {
  if (!res.headersSent) json(res, e.status || 500, { error: e.message }); else res.destroy();
})).listen(PORT, () => console.log(`Web UI: http://localhost:${PORT}  ->  TCP ${HOST}:${CPORT}`));
