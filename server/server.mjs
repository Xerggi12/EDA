import http from 'node:http';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { readFile } from 'node:fs/promises';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const executable = process.env.EDA_CORE ?? path.join(root, 'build', process.platform === 'win32' ? 'registro-personas.exe' : 'registro-personas');
const directory = path.resolve(process.env.EDA_DATA_DIR ?? path.join(root, 'data'));
const host = '127.0.0.1';
const port = Number(process.env.PORT ?? 3000);
// Solo publicamos estos archivos de la interfaz, nunca la carpeta de datos.
const publicFiles = new Map([
  ['/', ['index.html', 'text/html; charset=utf-8']],
  ['/index.html', ['index.html', 'text/html; charset=utf-8']],
  ['/styles.css', ['styles.css', 'text/css; charset=utf-8']],
  ['/app.js', ['app.js', 'text/javascript; charset=utf-8']],
]);
const core = spawn(executable, [directory], { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
let sequence = 0, cachedStatus = null, alive = true, job = null, stopping = false;
let fatalError = null;
const pending = new Map();

function call(command, ...args) {
  if (!alive) return Promise.reject(new Error(fatalError ?? 'El motor C++ no esta disponible.'));
  if (pending.size >= 64) return Promise.reject(new Error('Demasiadas solicitudes pendientes.'));
  const id = ++sequence;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    const line = [id, command, ...args].map(value => encodeURIComponent(String(value))).join('\t') + '\n';
    core.stdin.write(line, error => {
      if (error) { pending.delete(id); reject(error); }
    });
  });
}

function coreFailed(error) {
  alive = false; fatalError = error.message;
  for (const request of pending.values()) request.reject(error);
  pending.clear();
  if (job?.state === 'running') { job.state = 'failed'; job.error = error.message; }
}
core.on('error', error => coreFailed(new Error(`No se pudo iniciar C++: ${error.message}. Ejecute npm run build.`)));
core.on('exit', (code, signal) => {
  coreFailed(new Error(fatalError ?? `C++ termino (codigo ${code}, senal ${signal ?? 'ninguna'}).`));
  if (!stopping) console.error(fatalError);
});
core.stdin.on('error', error => coreFailed(error));
core.stderr.on('data', chunk => { fatalError = chunk.toString().trim(); console.error(fatalError); });
createInterface({ input: core.stdout, crlfDelay: Infinity }).on('line', line => {
  try {
    const message = JSON.parse(line);
    if (message.event === 'ready') {
      cachedStatus = message.status;
      console.log(`Motor C++ listo: ${cachedStatus.total.toLocaleString('es-PE')} ciudadanos.`);
    } else if (message.event === 'progress') {
      if (job?.state === 'running') job.completed = message.count;
    } else {
      const request = pending.get(message.id);
      if (!request) return;
      pending.delete(message.id);
      if (message.ok) request.resolve(message.result);
      else request.reject(new Error(message.error));
    }
  } catch (error) { coreFailed(new Error(`Respuesta invalida del motor: ${error.message}`)); }
});

class HttpError extends Error {
  constructor(status, message) { super(message); this.status = status; }
}
function send(response, status, value) {
  if (response.destroyed || response.writableEnded) return;
  response.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
  response.end(JSON.stringify(value));
}
async function body(request) {
  if (!request.headers['content-type']?.toLowerCase().startsWith('application/json')) throw new HttpError(415, 'Se requiere Content-Type: application/json.');
  let size = 0, data = '';
  request.setEncoding('utf8');
  for await (const chunk of request) {
    size += Buffer.byteLength(chunk);
    if (size > 16384) throw new HttpError(413, 'Solicitud demasiado grande.');
    data += chunk;
  }
  let value;
  try { value = JSON.parse(data); }
  catch { throw new HttpError(400, 'JSON invalido.'); }
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new HttpError(400, 'JSON invalido.');
  available();
  return value;
}
function integer(value, minimum, maximum, label) {
  if (!Number.isSafeInteger(value) || value < minimum || value > maximum) throw new HttpError(400, `${label}: entero entre ${minimum} y ${maximum}.`);
  return value;
}
function string(value, label, optional = false) {
  if (optional && value === undefined) return '';
  if (typeof value !== 'string' || (!optional && !value.trim()) || Buffer.byteLength(value) > 512 || /[\u0000-\u001f\u007f]/u.test(value)) throw new HttpError(400, `${label}: texto valido de hasta 512 bytes.`);
  return value.trim();
}
function available() {
  if (!alive) throw new HttpError(503, fatalError ?? 'Motor no disponible.');
  if (!cachedStatus) throw new HttpError(503, 'Cargando registros en memoria.');
  if (job?.state === 'running') throw new HttpError(409, 'Hay una tarea masiva en curso; consulte /api/status.');
}
function startJob(type, operation) {
  const current = { id: `${Date.now()}-${sequence + 1}`, type, state: 'running', startedAt: new Date().toISOString(), completed: cachedStatus.total };
  job = current;
  operation().then(async result => {
    current.result = result;
    cachedStatus = await call('status');
    current.state = 'completed'; current.finishedAt = new Date().toISOString();
  }).catch(async error => {
    // A generation can have committed part of its target before a later error.
    if (alive) { try { cachedStatus = await call('status'); } catch { /* retain the last known status */ } }
    current.state = 'failed'; current.error = error.message; current.finishedAt = new Date().toISOString();
  });
  return current;
}

const civil = { soltero: 0, casado: 1, divorciado: 2, viudo: 3, conviviente: 4 };
const ubigeos = [
  { code: '150101', department: 'Lima', province: 'Lima', district: 'Lima' },
  { code: '150122', department: 'Lima', province: 'Lima', district: 'Miraflores' },
  { code: '040101', department: 'Arequipa', province: 'Arequipa', district: 'Arequipa' },
  { code: '080101', department: 'Cusco', province: 'Cusco', district: 'Cusco' },
  { code: '130101', department: 'La Libertad', province: 'Trujillo', district: 'Trujillo' },
];

const server = http.createServer(async (request, response) => {
  try {
    if (request.headers.origin && request.headers.origin !== `http://${request.headers.host}`) throw new HttpError(403, 'Origen no permitido.');
    const url = new URL(request.url, `http://${host}`);
    const route = url.pathname;
    const method = request.method;
    if (method === 'GET' && route === '/api/disponibilidad') {
      return send(response, 200, { ready: Boolean(cachedStatus) && alive,
        busy: job?.state === 'running', error: alive ? null : fatalError });
    }
    if (method === 'GET' && route === '/api/status') {
      if (alive && cachedStatus && job?.state !== 'running') cachedStatus = await call('status');
      return send(response, 200, { ready: Boolean(cachedStatus) && alive, loading: !cachedStatus && alive, status: cachedStatus, job, error: alive ? null : fatalError });
    }
    if ((method === 'GET' || method === 'HEAD') && publicFiles.has(route)) {
      const [file, contentType] = publicFiles.get(route);
      const content = await readFile(path.join(root, 'public', file));
      response.writeHead(200, {
        'Content-Type': contentType,
        'Content-Length': content.length,
        'Cache-Control': 'no-store',
        'X-Content-Type-Options': 'nosniff',
        'Content-Security-Policy': "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
      });
      return response.end(method === 'HEAD' ? undefined : content);
    }
    if (method === 'GET' && route === '/api/ubigeos') return send(response, 200, { scope: 'catalogo inicial de cinco distritos; no es el catalogo nacional completo', items: ubigeos });
    available();
    if (method === 'GET' && route === '/api/citizens') {
      const dni = url.searchParams.get('dni') ?? '';
      if (dni && !/^\d{8}$/.test(dni)) throw new HttpError(400, 'El DNI debe tener ocho dígitos.');
      const filtros = ['firstNames', 'paternalSurname', 'maternalSurname'].map(
        campo => string(url.searchParams.get(campo) ?? '', campo, true));
      const inicio = integer(Number(url.searchParams.get('offset') ?? 0), 0, 100000000, 'offset');
      const limite = integer(Number(url.searchParams.get('limit') ?? 20), 1, 50, 'limit');
      return send(response, 200, await call('list', dni, ...filtros, inicio, limite));
    }
    if (method === 'GET' && route === '/api/citizens/sample') {
      const limit = integer(Number(url.searchParams.get('limit') ?? 10), 1, 100, 'limit');
      return send(response, 200, { items: await call('sample', limit) });
    }
    if (method === 'GET' && (route === '/api/citizens/search' ||
        (route === '/api/suggestions' && url.searchParams.has('field')))) {
      const field = ['firstNames', 'paternalSurname', 'maternalSurname'].indexOf(url.searchParams.get('field'));
      if (field < 0) throw new HttpError(400, 'Seleccione un criterio de búsqueda.');
      const q = string(url.searchParams.get('q'), 'Búsqueda');
      const limit = integer(Number(url.searchParams.get('limit') ?? 20), 1, 50, 'limit');
      const offset = integer(Number(url.searchParams.get('offset') ?? 0), 0, 100000000, 'offset');
      if (route === '/api/suggestions') return send(response, 200, { items: await call('suggest-field', field, q, 0, limit) });
      return send(response, 200, await call('search', field, q, offset, limit));
    }
    if (method === 'GET' && route === '/api/suggestions') {
      const q = string(url.searchParams.get('q') ?? '', 'q', true);
      const limit = integer(Number(url.searchParams.get('limit') ?? 10), 1, 50, 'limit');
      return send(response, 200, { kind: 'name-terms', items: await call('suggest', q, limit) });
    }
    const citizenMatch = /^\/api\/citizens\/(\d{8})$/.exec(route);
    const historyMatch = /^\/api\/citizens\/(\d{8})\/history$/.exec(route);
    if (method === 'GET' && historyMatch) {
      if (!await call('find', historyMatch[1])) throw new HttpError(404, 'La persona no esta registrada.');
      return send(response, 200, { items: await call('history', historyMatch[1]) });
    }
    if (method === 'GET' && citizenMatch) {
      const citizen = await call('find', citizenMatch[1]);
      if (!citizen) throw new HttpError(404, 'La persona no esta registrada.');
      return send(response, 200, citizen);
    }
    const deathMatch = /^\/api\/citizens\/(\d{8})\/death$/.exec(route);
    if (method === 'POST' && deathMatch) {
      await body(request);
      const citizen = await call('find', deathMatch[1]);
      if (!citizen) throw new HttpError(404, 'La persona no esta registrada.');
      const result = await call('death', deathMatch[1]);
      cachedStatus = await call('status');
      return send(response, 200, result);
    }
    if ((method === 'POST' && route === '/api/citizens') || (method === 'PUT' && citizenMatch)) {
      const value = await body(request);
      const editing = method === 'PUT';
      const allowed = ['firstNames', 'paternalSurname', 'maternalSurname', 'nationality', 'birthUbigeo', 'civilStatus', 'phone', 'email', 'address'];
      if (editing) allowed.push('reason', 'reference');
      if (Object.keys(value).some(key => !allowed.includes(key))) throw new HttpError(400, 'Campo no permitido: el DNI y el estado vital no se editan.');
      if (editing && !await call('find', citizenMatch[1])) throw new HttpError(404, 'La persona no esta registrada.');
      const address = value.address;
      if (!address || typeof address !== 'object' || Array.isArray(address)) throw new HttpError(400, 'Falta la direccion.');
      if (Object.keys(address).some(key => !['ubigeo', 'city', 'street', 'number', 'detail'].includes(key))) throw new HttpError(400, 'Campo de direccion no permitido.');
      if (typeof value.civilStatus !== 'string' || !Object.hasOwn(civil, value.civilStatus)) throw new HttpError(400, 'Estado civil invalido.');
      for (const code of [value.birthUbigeo, address.ubigeo]) if (typeof code !== 'string' || !/^\d{6}$/.test(code) || code === '000000') throw new HttpError(400, 'Ubigeo: cadena de seis digitos.');
      const fields = [string(value.firstNames, 'firstNames'), string(value.paternalSurname, 'paternalSurname'), string(value.maternalSurname, 'maternalSurname'),
        string(value.nationality, 'nationality'), value.birthUbigeo, address.ubigeo, string(address.city, 'city'), string(address.street, 'street'),
        integer(address.number, 0, 999999, 'number'), string(address.detail, 'detail', true), string(value.phone, 'phone'), string(value.email, 'email'), civil[value.civilStatus], ''];
      let result;
      if (editing) {
        fields[fields.length - 1] = citizenMatch[1];
        result = await call('update', ...fields, string(value.reason, 'Motivo'), string(value.reference, 'Referencia documental', true));
      } else {
        result = await call('register', ...fields);
      }
      cachedStatus = await call('status');
      return send(response, editing ? 200 : 201, result);
    }
    if (method === 'POST' && route === '/api/generate') {
      const value = await body(request);
      const target = integer(value.target, cachedStatus.total, 100000000, 'target');
      const seed = integer(value.seed ?? 202602, 0, Number.MAX_SAFE_INTEGER, 'seed');
      const current = startJob('generation', () => call('generate', target, seed));
      current.target = target;
      return send(response, 202, { jobId: current.id, status: '/api/status' });
    }
    if (method === 'POST' && route === '/api/flush') {
      await body(request); await call('flush'); return send(response, 200, { saved: true });
    }
    if (method === 'POST' && route === '/api/verify') {
      await body(request);
      const current = startJob('verification', () => call('verify'));
      return send(response, 202, { jobId: current.id, status: '/api/status' });
    }
    throw new HttpError(404, 'Ruta inexistente.');
  } catch (error) { send(response, error.status ?? 400, { error: error.message }); }
});
server.requestTimeout = 30000;
server.on('error', error => { console.error(error.message); core.kill(); process.exitCode = 1; });
server.listen(port, host, () => console.log(`API local: http://${host}:${server.address().port}`));

async function shutdown() {
  if (stopping) return;
  stopping = true;
  server.close();
  if (alive) { try { await call('quit'); } catch { core.kill(); } }
  if (process.connected) process.disconnect();
}
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
process.on('message', message => { if (message === 'shutdown') shutdown(); });
