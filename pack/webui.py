#!/usr/bin/env python3
"""
Web UI for ./server_api  (zero dependencies, Python 3.8+, Linux/macOS)

Usage:   python3 webui.py [/path/to/server_api] [port]
Open:    http://127.0.0.1:8080

How it works: each browser session spawns its own ./server_api process on a
pseudo-terminal, walks the text menus for you, and returns the output as JSON.
Files the CLI creates (sync / stream_genre) are detected and offered for download.
"""
import os, sys, re, pty, time, json, select, secrets, threading, termios
import subprocess, tempfile, shutil
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

BIN = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./server_api")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
CWD = os.path.dirname(BIN)

PROMPT = re.compile(r"(Choose option \[1-3\]: |@app> |Username: |Password: )\s*$")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
SAFE_NAME = re.compile(r"[^A-Za-z0-9._-]")
COMMANDS = {  # command -> number of args
    "sync": 0, "stream_genre": 1, "add_user": 2, "update_user": 2,
    "delete_user": 1, "delete_file": 1, "delete_all_user": 1, "logout": 0,
}


def clean_arg(v):
    v = str(v or "").strip()
    if not v or re.search(r"\s", v):
        raise ValueError("Values must be non-empty and contain no spaces/newlines (the CLI splits on whitespace).")
    return v


def snapshot(limit=20000):
    snap, n = {}, 0
    for root, _, files in os.walk(CWD):
        for f in files:
            p = os.path.join(root, f)
            try:
                st = os.stat(p)
            except OSError:
                continue
            snap[p] = (st.st_mtime, st.st_size)
            n += 1
            if n >= limit:
                return snap
    return snap


class Session:
    def __init__(self):
        self.lock = threading.Lock()
        self.user = None
        self.files = {}  # id -> path (downloadable)
        self.tmp = tempfile.mkdtemp(prefix="webui_")
        self.proc = None
        self.start()

    def start(self):
        master, slave = pty.openpty()
        attr = termios.tcgetattr(slave)
        attr[3] &= ~termios.ECHO
        termios.tcsetattr(slave, termios.TCSANOW, attr)
        self.proc = subprocess.Popen([BIN], stdin=slave, stdout=slave, stderr=slave,
                                     cwd=CWD, close_fds=True)
        os.close(slave)
        self.fd = master
        self.user = None
        self.read()

    def alive(self):
        return self.proc.poll() is None

    def read(self, timeout=60, idle=1.5):
        buf, start = b"", time.time()
        last = start
        while time.time() - start < timeout:
            r, _, _ = select.select([self.fd], [], [], 0.2)
            if r:
                try:
                    d = os.read(self.fd, 4096)
                except OSError:
                    break
                if not d:
                    break
                buf += d
                last = time.time()
                if PROMPT.search(ANSI.sub("", buf.decode(errors="replace")).replace("\r", "")):
                    break
            else:
                if not self.alive():
                    break
                if buf and time.time() - last > idle:
                    break
        return ANSI.sub("", buf.decode(errors="replace")).replace("\r", "")

    def write(self, line):
        os.write(self.fd, (line + "\n").encode())

    def ensure(self):
        if not self.alive():
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.start()

    # ---- high level actions ----
    def auth(self, mode, username, password):
        self.ensure()
        if self.user:
            self.write("logout"); self.read()
        out = ""
        self.write("1" if mode == "login" else "2"); out += self.read()
        self.write(username); out += self.read()
        self.write(password); out += self.read()
        if mode == "login" and "@app>" in out[-20:] and "Logged in successfully" in out:
            self.user = username
        return out

    def command(self, line, cleanup=None):
        self.ensure()
        if not self.user:
            return "[WEB] Not logged in."
        before = snapshot()
        self.write(line)
        out = self.read()
        if cleanup and os.path.exists(cleanup):
            os.remove(cleanup)
        if "Main Menu" in out and "@app>" not in out[-20:]:
            self.user = None
        # register new/changed files
        new = []
        for p, meta in snapshot().items():
            if before.get(p) != meta:
                fid = secrets.token_hex(6)
                self.files[fid] = p
                new.append({"id": fid, "name": os.path.relpath(p, CWD)})
        self.new_files = new
        return out

    def close(self):
        try:
            self.proc.terminate()
        except Exception:
            pass
        shutil.rmtree(self.tmp, ignore_errors=True)


SESSIONS = {}
STAGE_LOCK = threading.Lock()


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def sess(self, create=True):
        c = self.headers.get("Cookie", "")
        m = re.search(r"sid=([0-9a-f]+)", c)
        sid = m.group(1) if m else None
        if sid in SESSIONS:
            return sid, SESSIONS[sid]
        if not create:
            return None, None
        sid = secrets.token_hex(16)
        SESSIONS[sid] = Session()
        return sid, SESSIONS[sid]

    def send_json(self, obj, code=200, sid=None):
        data = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        if sid:
            self.send_header("Set-Cookie", f"sid={sid}; Path=/; HttpOnly; SameSite=Strict")
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            data = HTML.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        elif u.path == "/api/state":
            sid, s = self.sess()
            self.send_json({"user": s.user}, sid=sid)
        elif u.path == "/api/download":
            _, s = self.sess(False)
            fid = parse_qs(u.query).get("id", [""])[0]
            p = s.files.get(fid) if s else None
            if not p or not os.path.isfile(p):
                return self.send_json({"error": "not found"}, 404)
            size = os.path.getsize(p)
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size))
            self.send_header("Content-Disposition", f'attachment; filename="{os.path.basename(p)}"')
            self.end_headers()
            with open(p, "rb") as f:
                shutil.copyfileobj(f, self.wfile)
        else:
            self.send_json({"error": "not found"}, 404)

    def do_POST(self):
        u = urlparse(self.path)
        sid, s = self.sess()
        try:
            if u.path in ("/api/login", "/api/signup"):
                d = json.loads(self.body() or b"{}")
                un, pw = clean_arg(d.get("username")), clean_arg(d.get("password"))
                with s.lock:
                    out = s.auth("login" if u.path.endswith("login") else "signup", un, pw)
                return self.send_json({"user": s.user, "output": out}, sid=sid)

            if u.path == "/api/cmd":
                d = json.loads(self.body() or b"{}")
                cmd = d.get("cmd")
                if cmd not in COMMANDS:
                    raise ValueError("Unknown command")
                args = [clean_arg(a) for a in (d.get("args") or [])]
                if len(args) != COMMANDS[cmd]:
                    raise ValueError(f"{cmd} needs {COMMANDS[cmd]} argument(s)")
                with s.lock:
                    out = s.command(" ".join([cmd] + args))
                    return self.send_json({"user": s.user, "output": out,
                                           "files": getattr(s, "new_files", [])}, sid=sid)

            if u.path == "/api/send":
                q = {k: v[0] for k, v in parse_qs(u.query).items()}
                recipient = clean_arg(q.get("recipient"))
                name = SAFE_NAME.sub("_", q.get("name", "upload.bin"))[:100] or "upload.bin"
                data = self.body()
                # The CLI uses the <local_file> argument as the stored name too,
                # so stage the file in its cwd and pass only the bare filename.
                path = os.path.join(CWD, name)
                with STAGE_LOCK:
                    backup = None
                    if os.path.exists(path):  # keep the user's existing file safe
                        backup = os.path.join(s.tmp, "orig_" + name)
                        shutil.move(path, backup)
                    try:
                        with open(path, "wb") as f:
                            f.write(data)
                        if recipient == "admin":
                            line = f"send admin {name} {clean_arg(q.get('genre'))}"
                        else:
                            line = f"send {recipient} {name}"
                        with s.lock:
                            out = s.command(line, cleanup=path)
                    finally:
                        if os.path.exists(path):
                            os.remove(path)
                        if backup and os.path.exists(backup):
                            shutil.move(backup, path)
                return self.send_json({"user": s.user, "output": out, "files": []}, sid=sid)
        except (ValueError, json.JSONDecodeError) as e:
            return self.send_json({"error": str(e)}, 400, sid=sid)
        self.send_json({"error": "not found"}, 404)


HTML = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wrapper Console</title>
<style>
:root{--bg:#0e1116;--card:#161b22;--line:#2a313c;--fg:#e6edf3;--mut:#8b949e;--acc:#3fb950;--bad:#f85149;--blue:#58a6ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,sans-serif}
header{display:flex;justify-content:space-between;align-items:center;padding:14px 22px;border-bottom:1px solid var(--line)}
h1{font-size:16px;margin:0;letter-spacing:.5px}.pill{background:var(--card);border:1px solid var(--line);border-radius:99px;padding:3px 12px;color:var(--mut)}
main{max-width:1000px;margin:24px auto;padding:0 16px;display:grid;gap:16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(290px,1fr));gap:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:16px}
.card h2{font-size:13px;text-transform:uppercase;letter-spacing:.8px;color:var(--mut);margin:0 0 12px}
input,select{width:100%;background:var(--bg);color:var(--fg);border:1px solid var(--line);border-radius:6px;padding:8px 10px;margin-bottom:8px;font:inherit}
input[type=file]{padding:6px}
button{background:var(--acc);color:#04130a;border:0;border-radius:6px;padding:8px 14px;font:600 13px inherit;cursor:pointer}
button.sec{background:#21262d;color:var(--fg);border:1px solid var(--line)}button.danger{background:var(--bad);color:#fff}
button:disabled{opacity:.5;cursor:wait}.row{display:flex;gap:8px;flex-wrap:wrap}
#auth{max-width:380px;margin:60px auto}
pre{margin:0;background:#010409;border:1px solid var(--line);border-radius:8px;padding:12px;max-height:340px;overflow:auto;white-space:pre-wrap;font:12.5px/1.45 ui-monospace,monospace;color:#c9d1d9}
.files a{display:block;color:var(--blue);text-decoration:none;padding:4px 0}.hint{color:var(--mut);font-size:12px;margin:6px 0 0}
.err{color:var(--bad);margin-top:8px}.hidden{display:none}
</style></head><body>
<header><h1>▍WRAPPER CLI · WEB CONSOLE</h1><span class="pill" id="who">signed out</span></header>
<main>
 <section id="auth" class="card">
  <h2>Authentication</h2>
  <input id="u" placeholder="Username" autocomplete="username">
  <input id="p" type="password" placeholder="Password" autocomplete="current-password">
  <div class="row"><button onclick="auth('login')">Sign in</button><button class="sec" onclick="auth('signup')">Sign up</button></div>
  <div class="err" id="autherr"></div>
 </section>

 <section id="app" class="hidden">
  <div class="grid">
   <div class="card"><h2>Mailbox</h2>
     <button onclick="cmd('sync')">Sync pending files</button>
     <div class="files" id="files"></div></div>

   <div class="card"><h2>Send file</h2>
     <input id="s_rec" placeholder="Recipient username (or 'admin')">
     <input id="s_genre" class="hidden" placeholder="Genre (admin video only)">
     <input id="s_file" type="file">
     <button onclick="sendFile()">Send</button></div>

   <div class="card"><h2>Stream from admin</h2>
     <input id="g" placeholder="Genre">
     <button onclick="cmd('stream_genre',[v('g')])">Stream to my storage</button></div>

   <div class="card"><h2>Accounts</h2>
     <input id="a_u" placeholder="Username"><input id="a_p" type="password" placeholder="Password / new password">
     <div class="row">
      <button onclick="cmd('add_user',[v('a_u'),v('a_p')])">Add user</button>
      <button class="sec" onclick="cmd('update_user',[v('a_u'),v('a_p')])">Update password</button>
      <button class="danger" onclick="confirm('Delete this user?')&&cmd('delete_user',[v('a_u')])">Delete user</button></div></div>

   <div class="card"><h2>Admin</h2>
     <input id="d_f" placeholder="Filename">
     <button class="danger" onclick="confirm('Delete file globally?')&&cmd('delete_file',[v('d_f')])">Delete file</button>
     <input id="d_u" placeholder="Username or 'admin'" style="margin-top:12px">
     <button class="danger" onclick="confirm('Clear all of this repository?')&&cmd('delete_all_user',[v('d_u')])">Clear mailbox / repo</button>
     <p class="hint">Server enforces admin rights.</p></div>

   <div class="card"><h2>Session</h2><button class="sec" onclick="cmd('logout')">Log out</button></div>
  </div>
 </section>

 <section class="card"><h2>Console output</h2><pre id="log">Ready.</pre></section>
</main>
<script>
const $=id=>document.getElementById(id), v=id=>$(id).value.trim();
const log=t=>{const l=$('log');l.textContent+='\n'+t.trim();l.scrollTop=l.scrollHeight};
function setUser(u){$('who').textContent=u?('signed in: '+u):'signed out';
  $('auth').classList.toggle('hidden',!!u);$('app').classList.toggle('hidden',!u)}
async function call(path,opt){
  const btns=[...document.querySelectorAll('button')];btns.forEach(b=>b.disabled=true);
  try{const r=await fetch(path,opt);const d=await r.json();
    if(d.error){log('[WEB] '+d.error);$('autherr').textContent=d.error;return d}
    return d}
  catch(e){log('[WEB] '+e);return {}}
  finally{btns.forEach(b=>b.disabled=false)}
}
const post=(p,b)=>call(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
async function auth(mode){
  $('autherr').textContent='';
  const d=await post('/api/'+mode,{username:v('u'),password:v('p')});
  if(d.output){log(d.output);if(!d.user)$('autherr').textContent=mode==='login'?'Login failed.':'See console for sign-up result.'}
  setUser(d.user||null);
}
async function cmd(c,args=[]){
  const d=await post('/api/cmd',{cmd:c,args});
  if(d.output)log('$ '+c+' '+args.filter(a=>!/pass/i.test(c)).join(' ')+'\n'+d.output);
  if(d.files&&d.files.length){$('files').innerHTML=d.files.map(f=>`<a href="/api/download?id=${f.id}" download>⬇ ${f.name}</a>`).join('')}
  if('user' in d)setUser(d.user);
}
async function sendFile(){
  const f=$('s_file').files[0],rec=v('s_rec');
  if(!f||!rec){log('[WEB] Pick a file and recipient.');return}
  const q=new URLSearchParams({recipient:rec,name:f.name,genre:v('s_genre')});
  const d=await call('/api/send?'+q,{method:'POST',body:f});
  if(d.output)log('$ send '+rec+' '+f.name+'\n'+d.output);
}
$('s_rec').addEventListener('input',()=>$('s_genre').classList.toggle('hidden',v('s_rec')!=='admin'));
document.addEventListener('keydown',e=>{if(e.key==='Enter'&&!$('auth').classList.contains('hidden'))auth('login')});
fetch('/api/state').then(r=>r.json()).then(d=>setUser(d.user));
</script></body></html>'''


if __name__ == "__main__":
    if not os.access(BIN, os.X_OK):
        sys.exit(f"Cannot execute {BIN}. Pass the path: python3 webui.py /path/to/server_api")
    print(f"Serving on http://127.0.0.1:{PORT}  (wrapping {BIN})")
    try:
        ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
    except KeyboardInterrupt:
        for s in SESSIONS.values():
            s.close()
