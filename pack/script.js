const $ = s => document.querySelector(s);
const escapeHtml = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const fmt = n => n < 1024 ? n + ' B' : n < 1048576 ? (n / 1024).toFixed(1) + ' KB' : (n / 1048576).toFixed(1) + ' MB';
let auth = JSON.parse(sessionStorage.getItem('auth') || 'null'), cur = null;
 
async function api(url, body, raw) {
  const r = await fetch(url, {
    method: body ? 'POST' : 'GET',
    headers: { 'X-Token': auth.token, 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined,
  });
  if (raw && r.ok) return r;
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || r.statusText);
  return j;
}
const run = line => api('/api/term', { line }).then(j => j.reply);
 
function show() {
  $('#login').hidden = !!auth; $('#app').hidden = !auth;
  if (auth) { $('#who').textContent = auth.username; loadUsers(); }
}
 
$('#loginForm').onsubmit = async e => {
  e.preventDefault();
  $('#loginErr').textContent = 'Signing in…';
  try {
    const username = $('#lu').value;
    const r = await fetch('/api/login', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ username, password: $('#lp').value }) });
    const j = await r.json(); if (!r.ok) throw new Error(j.error);
    auth = { username, token: j.token }; sessionStorage.setItem('auth', JSON.stringify(auth)); $('#loginErr').textContent = ''; show(); }
  catch (err) { auth = null; $('#loginErr').textContent = err.message; }
};
$('#logout').onclick = () => { api('/api/logout', {}).catch(() => {}); auth = null; sessionStorage.removeItem('auth'); show(); };
 
document.querySelectorAll('nav button').forEach(b => b.onclick = () => {
  document.querySelectorAll('nav button').forEach(x => x.classList.toggle('on', x === b));
  document.querySelectorAll('.view').forEach(v => v.hidden = v.id !== b.dataset.v);
  if (b.dataset.v === 'admin') loadMeta();
  if (b.dataset.v === 'terminal') $('#cmd').focus();
});
 
// ---- Workspaces ----
async function loadUsers() {
  try {
    const { users } = await api('/api/users');
    const list = users;
    $('#users').innerHTML = list.map(u => `<li data-u="${escapeHtml(u)}" class="${u === cur ? 'on' : ''}">${escapeHtml(u)}</li>`).join('');
    document.querySelectorAll('#users li').forEach(li => li.onclick = () => { cur = li.dataset.u; loadUsers(); loadFiles(); });
  } catch (e) { $('#users').innerHTML = `<li class="err">${escapeHtml(e.message)}</li>`; }
}
async function loadFiles() {
  if (!cur) return;
  $('#wsTitle').textContent = `storage / ${cur === 'admin' ? 'admin' : 'users/' + cur}`;
  const { files } = await api('/api/files?user=' + encodeURIComponent(cur));
  $('#files').innerHTML = files.length ? files.map(f =>
    `<tr><td>${escapeHtml(f.name)}</td><td>${escapeHtml((f.name.split('.').pop() || '').toUpperCase())}</td><td>${fmt(f.size)}</td>
     <td><button class="ghost" data-f="${escapeHtml(f.name)}">Download</button></td></tr>`).join('')
    : '<tr><td colspan="4">Empty mailbox</td></tr>';
  document.querySelectorAll('#files button').forEach(b => b.onclick = async () => {
    const r = await api(`/api/download?user=${encodeURIComponent(cur)}&file=${encodeURIComponent(b.dataset.f)}`, null, true);
    const a = document.createElement('a'); a.href = URL.createObjectURL(await r.blob()); a.download = b.dataset.f; a.click();
  });
}
$('#sync').onclick = async () => { await run('sync').catch(() => {}); loadUsers(); loadFiles(); };
 
// ---- Admin repository ----
async function loadMeta() {
  try {
    const { rows } = await api('/api/meta');
    $('#meta').innerHTML = rows.length ? rows.map(r =>
      `<tr><td>${escapeHtml(r[0])}</td><td>${escapeHtml(r[1])}</td><td>${escapeHtml(r[2])}</td><td>${escapeHtml(r[3])}</td>
       <td><button class="del" data-f="${escapeHtml(r[1])}">Delete</button></td></tr>`).join('') : '<tr><td colspan="5">No records</td></tr>';
    document.querySelectorAll('#meta .del').forEach(b => b.onclick = async () => {
      if (!confirm(`Delete ${b.dataset.f}? This removes the file and its database record.`)) return;
      $('#genreOut').textContent = await run('delete_file ' + b.dataset.f); loadMeta();
    });
  } catch (e) { $('#meta').innerHTML = `<tr><td colspan="5" class="err">${escapeHtml(e.message)}</td></tr>`; }
}
$('#reloadMeta').onclick = loadMeta;
$('#genreForm').onsubmit = async e => {
  e.preventDefault(); $('#genreOut').textContent = 'Searching…';
  try { $('#genreOut').textContent = await run('stream_genre ' + $('#genre').value); loadUsers(); } catch (err) { $('#genreOut').textContent = err.message; }
};
 
// ---- Terminal ----
function print(text, cls = '') { const d = document.createElement('div'); d.className = cls; d.textContent = text; $('#screen').append(d); $('#screen').scrollTop = 1e9; }
print('ZShare daemon shell. Commands: add_user, update_user, delete_user, delete_file, stream_genre, sync, clear', 'ok');
$('#termForm').onsubmit = async e => {
  e.preventDefault(); const line = $('#cmd').value.trim(); $('#cmd').value = ''; if (!line) return;
  print('> ' + line, 'cmd');
  if (line === 'clear') { $('#screen').innerHTML = ''; return; }
  try { const r = await run(line); print(r, /FAIL|NOT_FOUND|Unknown/.test(r) ? 'bad' : 'ok'); }
  catch (err) { print(err.message, 'bad'); }
};
 
show();
 
// ---- Send / upload (runs: send <recipient> <file> [genre]) ----
$('#sendForm').onsubmit = async e => {
  e.preventDefault(); const f = $('#sendFile').files[0]; $('#sendOut').textContent = 'Sending…';
  try {
    const r = await fetch('/api/send', { method: 'POST', body: f, headers: { 'X-Token': auth.token, 'X-To': encodeURIComponent($('#sendTo').value.trim()),
      'X-Name': encodeURIComponent(f.name), 'X-Genre': encodeURIComponent($('#sendGenre').value.trim()) } });
    const j = await r.json(); if (!r.ok) throw new Error(j.error);
    $('#sendOut').textContent = j.reply; loadUsers(); loadFiles();
  } catch (err) { $('#sendOut').textContent = err.message; }
};
 
