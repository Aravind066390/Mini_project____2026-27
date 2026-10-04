const $ = s => document.querySelector(s);
let auth = JSON.parse(sessionStorage.getItem('auth') || 'null');

function log(msg, cls = '') {
  const d = document.createElement('div');
  d.className = cls;
  d.textContent = `${new Date().toLocaleTimeString()}  ${msg}`;
  $('#log').prepend(d);
}

async function post(url, body) {
  const r = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ...auth, ...body }) });
  if (!r.ok) { const j = await r.json().catch(() => ({})); throw new Error(j.error || r.statusText); }
  return r;
}

const command = async (action, args) => (await (await post('/api/command', { action, args })).json()).reply;

function show() {
  const on = !!auth;
  $('#loginView').hidden = on; $('#appView').hidden = !on; $('#who').hidden = !on;
  if (on) $('#whoName').textContent = auth.username;
}

// ---- Login ----
$('#loginForm').onsubmit = async e => {
  e.preventDefault();
  auth = { username: $('#lu').value, password: $('#lp').value };
  try {
    await post('/api/login', {});
    sessionStorage.setItem('auth', JSON.stringify(auth));
    log('LOGIN_OK', 'ok'); show();
  } catch (err) { auth = null; log(err.message, 'err'); }
};
$('#logout').onclick = () => { auth = null; sessionStorage.removeItem('auth'); show(); };

// ---- Tabs ----
document.querySelectorAll('nav button').forEach(b => b.onclick = () => {
  document.querySelectorAll('nav button').forEach(x => x.classList.toggle('active', x === b));
  document.querySelectorAll('.tab').forEach(t => t.hidden = t.id !== b.dataset.tab);
});

// ---- Search & stream ----
$('#searchForm').onsubmit = async e => {
  e.preventDefault();
  const btn = e.submitter, bar = $('#progress'), fill = bar.firstElementChild, out = $('#player');
  btn.disabled = true; out.innerHTML = ''; fill.style.width = '0'; bar.hidden = false;
  try {
    log(`SEARCH_GENRE ${$('#genre').value}`);
    const r = await post('/api/stream', { genre: $('#genre').value });
    const file = decodeURIComponent(r.headers.get('X-File-Path') || '');
    const type = r.headers.get('Content-Type') || 'application/octet-stream';
    log(`FILE_FOUND ${file}`, 'ok');
    // read the stream chunk by chunk so we can show progress (size is unknown, so animate)
    const reader = r.body.getReader(), chunks = []; let got = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      chunks.push(value); got += value.length;
      fill.style.width = Math.min(95, 100 * (1 - Math.exp(-got / 2e7))) + '%';
    }
    fill.style.width = '100%';
    const url = URL.createObjectURL(new Blob(chunks, { type }));
    const name = file.split('/').pop() || 'media';
    const el = type.startsWith('video') ? document.createElement('video') : type.startsWith('audio') ? document.createElement('audio') : null;
    if (el) { el.src = url; el.controls = true; out.append(el); }
    out.insertAdjacentHTML('beforeend', `<p>${name.replace(/[<>&]/g, '')} · ${(got / 1048576).toFixed(1)} MB · <a href="${url}" download="${name.replace(/"/g, '')}">Download</a></p>`);
    log(`Streamed ${got} bytes`, 'ok');
  } catch (err) { out.innerHTML = `<p>${err.message}</p>`; log(err.message, 'err'); }
  btn.disabled = false; setTimeout(() => bar.hidden = true, 600);
};

// ---- Upload (XHR for upload progress) ----
$('#uploadForm').onsubmit = e => {
  e.preventDefault();
  const f = $('#file').files[0], rcp = $('#recipient').value.trim(), bar = $('#upBar'), fill = bar.firstElementChild;
  const xhr = new XMLHttpRequest();
  xhr.open('POST', `/api/put?recipient=${encodeURIComponent(rcp)}&filename=${encodeURIComponent(f.name.replace(/\s+/g, '_'))}`);
  xhr.setRequestHeader('X-User', encodeURIComponent(auth.username));
  xhr.setRequestHeader('X-Pass', encodeURIComponent(auth.password));
  bar.hidden = false; fill.style.width = '0';
  xhr.upload.onprogress = ev => fill.style.width = (100 * ev.loaded / ev.total) + '%';
  xhr.onload = () => {
    let j = {}; try { j = JSON.parse(xhr.responseText); } catch {}
    xhr.status === 200 ? log(`PUT ${rcp} ${f.name} → ${j.reply}`, 'ok') : log(j.error || 'Upload failed', 'err');
  };
  xhr.onerror = () => log('Upload failed (network)', 'err');
  log(`PUT ${rcp} ${f.name}`);
  xhr.send(f);
};

// ---- Delete file ----
$('#delForm').onsubmit = async e => {
  e.preventDefault();
  const name = $('#delName').value;
  if (!confirm(`Delete "${name}"?`)) return;
  try { log(`DELETE ${name} → ${await command('DELETE', [name])}`, 'ok'); } catch (err) { log(err.message, 'err'); }
};

// ---- Users ----
$('#userForm').onsubmit = async e => {
  e.preventDefault();
  const act = e.submitter.dataset.act, u = $('#uName').value, p = $('#uPass').value;
  if (act === 'DELETE_USER' && !confirm(`Delete user "${u}"?`)) return;
  try {
    const reply = await command(act, act === 'DELETE_USER' ? [u] : [u, p]);
    log(`${act} ${u} → ${reply}`, /FAIL/.test(reply) ? 'err' : 'ok');
  } catch (err) { log(err.message, 'err'); }
};

show();
