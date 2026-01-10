const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');
const { URL } = require('url');

const PORT = process.env.PORT ? Number(process.env.PORT) : 5173;
const TARGET = process.env.DEVICE_URL || 'http://192.168.10.10';
const PUBLIC_DIR = path.join(__dirname, 'public');

const MIME = {
  '.html': 'text/html',
  '.css': 'text/css',
  '.js': 'application/javascript',
  '.json': 'application/json',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
};

function send(res, status, body, headers = {}) {
  res.writeHead(status, { 'Content-Type': 'text/plain', ...headers });
  res.end(body);
}

function serveFile(res, filePath) {
  fs.stat(filePath, (err, stat) => {
    if (err || !stat.isFile()) {
      send(res, 404, 'Not found');
      return;
    }
    const ext = path.extname(filePath);
    const type = MIME[ext] || 'application/octet-stream';
    res.writeHead(200, { 'Content-Type': type });
    fs.createReadStream(filePath).pipe(res);
  });
}

function proxyRequest(req, res) {
  const targetUrl = new URL(TARGET);
  const proxyPath = req.url;
  const client = targetUrl.protocol === 'https:' ? https : http;

  const headers = { ...req.headers };
  headers.host = targetUrl.host;

  const options = {
    protocol: targetUrl.protocol,
    hostname: targetUrl.hostname,
    port: targetUrl.port || (targetUrl.protocol === 'https:' ? 443 : 80),
    method: req.method,
    path: proxyPath,
    headers,
  };

  const proxyReq = client.request(options, (proxyRes) => {
    res.writeHead(proxyRes.statusCode || 500, proxyRes.headers);
    proxyRes.pipe(res);
  });

  proxyReq.on('error', (err) => {
    send(res, 502, `Proxy error: ${err.message}`);
  });

  if (req.method === 'GET' || req.method === 'HEAD') {
    proxyReq.end();
    return;
  }

  req.pipe(proxyReq);
}

const server = http.createServer((req, res) => {
  if (req.url && req.url.startsWith('/api/')) {
    proxyRequest(req, res);
    return;
  }

  if (req.url === '/config') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ target: TARGET }));
    return;
  }

  const safePath = (req.url || '/').split('?')[0];
  const relPath = safePath === '/' ? '/index.html' : safePath;
  const filePath = path.normalize(path.join(PUBLIC_DIR, relPath));

  if (!filePath.startsWith(PUBLIC_DIR)) {
    send(res, 403, 'Forbidden');
    return;
  }

  serveFile(res, filePath);
});

server.listen(PORT, () => {
  console.log(`Device UI running at http://localhost:${PORT}`);
  console.log(`Proxy target: ${TARGET}`);
});
