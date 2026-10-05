// ZShare bridge: drives your ./server_api CLI (one process per logged-in web user) and serves the UI.
// Run from the folder that contains ./server_api:  node bridge.js  ->  http://localhost:3000
// Env: PORT, APP_DIR (where server_api lives), STORAGE_DIR, DATABASE_URL (psql), IDLE_MS (output-idle wait)
const http = require('http'), fs = require('fs'), os = require('os'), path = require('path'), crypto = require('crypto');
const { spawn, execFile } = require('child_process');
const PORT = +process.env.PORT || 3000, APP_DIR = path.resolve(process.env.APP_DIR || __dirname);
const STORAGE = path.resolve(process.env.STORAGE_DIR || path.join(APP_DIR, 'storage'));
const DB = process.env.DATABASE_URL || '', IDLE = +process.env.IDLE_MS || 1200;
const MIME = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript' };
process.on('uncaughtException', e => console.error('uncaught:', e));
process.on('unhandledRejection', e => console.error('unhandled:', e));
const strip = s => s.replace(/\x1b\[[0-9;]*[A-Za-z]/g, '').replace(/\r/g, '');
const clean = s => { if (typeof s !== 'string' || !s || /[\r\n]/.test(s)) throw new Error('Invalid input'); return s; };
const sessions = new Map();
 
// The CLI has no known end-of-output marker, so we treat "no output for IDLE ms" as "command finished".
const waitIdle = (s, max = 120000) => new Promise(res => {
  const t0 = Date.now(); s.last = Date.now();
  (function tick() {
    if (Date.now() - s.last >= IDLE || Date.now() - t0 > max) { const o = strip(s.buf); s.buf = ''; res(o); }
    else setTimeout(tick, 100);
  })();
});
const exec = (s, line) => {            // commands run one at a time per session
  const p = s.q.then(() => { s.proc.stdin.write(clean(line) + '\n'); return waitIdle(s); });
  s.q = p.catch(() => {}); return p;
};
 
async function login(user, pass) {
  clean(user); clean(pass);
  // stdbuf -o0 stops the C program holding prompts in a pipe buffer
  const proc = spawn('stdbuf', ['-o0', '-e0', './server_api'], { cwd: APP_DIR });
  const s = { proc, buf: '', last: Date.now(), q: Promise.resolve(), user };
  proc.stdout.on('data', d => { s.buf += d; s.last = Date.now(); });
  proc.stderr.on('data', d => { s.buf += d; s.last = Date.now(); });
  const dead = new Promise((_, rej) => proc.on('error', e => rej(new Error('Cannot start ./server_api: ' + e.message))));
  await Promise.race([waitIdle(s), dead]);                    // main menu
  proc.stdin.write(`1\n${user}\n${pass}\n`);
  const out = await Promise.race([waitIdle(s), dead]);
  console.log('login output:', JSON.stringify(out));
  if (!/Logged in successfully/i.test(out)) { proc.kill(); const e = new Error('Login failed'); e.status = 401; throw e; }
  const token = crypto.randomBytes(16).toString('hex');
  sessions.set(token, s); proc.on('exit', () => sessions.delete(token));
  return token;
}
const sess = req => { const s = sessions.get(req.headers['x-token']); if (!s) { const e = new Error('Not logged in'); e.status = 401; throw e; } return s; };
const body = req => new Promise((res, rej) => { let b = ''; req.on('data', d => b += d); req.on('end', () => { try { res(JSON.parse(b || '{}')); } catch (e) { rej(e); } }); });
const json = (res, code, o) => { res.writeHead(code, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(o)); };
const dirOf = (s, u) => {
  if (!/^[\w.-]+$/.test(u || '')) throw new Error('Bad user');
  if (s.user !== 'admin' && u !== s.user) { const e = new Error('Not allowed'); e.status = 403; throw e; }
  return u === 'admin' ? path.join(STORAGE, 'admin') : path.join(STORAGE, 'users', u);
};
 
async function handle(req, res) {
  const url = new URL(req.url, 'http://x'), P = url.pathname;
  if (P === '/api/login') { const b = await body(req); return json(res, 200, { token: await login(b.username, b.password) }); }
  if (P === '/api/logout') { const s = sess(req); s.proc.stdin.write('logout\n'); setTimeout(() => s.proc.kill(), 300); return json(res, 200, { ok: true }); }
  if (P === '/api/term') {
    const s = sess(req), b = await body(req);
    return json(res, 200, { reply: (await exec(s, b.line || '')).trim() || '(no output)' });
  }
  if (P === '/api/send') {                       // browser upload -> temp file -> "send <to> <file> [genre]"
    const s = sess(req), to = decodeURIComponent(req.headers['x-to'] || ''), genre = decodeURIComponent(req.headers['x-genre'] || '');
    const name = path.basename(decodeURIComponent(req.headers['x-name'] || '')).replace(/\s+/g, '_');
    if (!/^[\w.-]+$/.test(to) || !name) throw new Error('Bad recipient or filename');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zshare-')), f = path.join(dir, name);
    await new Promise((ok, no) => req.pipe(fs.createWriteStream(f)).on('finish', ok).on('error', no));
    const reply = await exec(s, `send ${to} ${f}${to === 'admin' && genre ? ' ' + genre : ''}`);
    fs.rmSync(dir, { recursive: true, force: true });
    return json(res, 200, { reply: reply.trim() });
  }
  if (P === '/api/users') {
    const s = sess(req), d = path.join(STORAGE, 'users');
    const all = fs.existsSync(d) ? fs.readdirSync(d) : [];
    return json(res, 200, { users: s.user === 'admin' ? ['admin', ...all] : [s.user] });
  }
  if (P === '/api/files') {
    const d = dirOf(sess(req), url.searchParams.get('user'));
    return json(res, 200, { files: fs.existsSync(d) ? fs.readdirSync(d).map(n => ({ name: n, size: fs.statSync(path.join(d, n)).size })) : [] });
  }
  if (P === '/api/download') {
    const f = path.join(dirOf(sess(req), url.searchParams.get('user')), path.basename(url.searchParams.get('file') || ''));
    if (!fs.existsSync(f)) return json(res, 404, { error: 'File not found' });
    res.writeHead(200, { 'Content-Type': 'application/octet-stream' }); return fs.createReadStream(f).pipe(res);
  }
  if (P === '/api/meta') {
    sess(req);
    if (!DB) return json(res, 500, { error: 'Set DATABASE_URL (e.g. postgresql://user:pass@localhost/db)' });
    return execFile('psql', [DB, '-At', '-F', '|', '-c', 'SELECT owner,file_name,original_name,genre FROM file_metadata ORDER BY 1,2'],
      (e, out, err) => e ? json(res, 500, { error: (err || e.message).trim() })
        : json(res, 200, { rows: out.trim().split('\n').filter(Boolean).map(l => l.split('|')) }));
  }
  const f = path.join(__dirname, P === '/' ? 'index.html' : P);
  if (!f.startsWith(__dirname) || !MIME[path.extname(f)]) { res.writeHead(404); return res.end('Not found'); }
  fs.readFile(f, (e, d) => { if (e) { res.writeHead(404); return res.end('Not found'); } res.writeHead(200, { 'Content-Type': MIME[path.extname(f)] }); res.end(d); });
}
http.createServer((req, res) => handle(req, res).catch(e => { console.error('request error:', e.message);
  if (!res.headersSent) json(res, e.status || 500, { error: e.message }); else res.destroy(); }))
  .listen(PORT, () => console.log(`ZShare: http://localhost:${PORT} -> ${APP_DIR}/server_api, storage ${STORAGE}`));
process.on('exit', () => sessions.forEach(s => s.proc.kill()));
 
