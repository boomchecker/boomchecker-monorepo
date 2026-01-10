# BOM Node Local UI

Minimal local UI that proxies API requests to the device.

## Run

```bash
DEVICE_URL=http://192.168.10.10 PORT=5173 node server.js
```

Then open `http://localhost:5173`.

## Notes
- Requests to `/api/*` are proxied to `DEVICE_URL`, so no CORS issues.
- Static files are served from `public/`.
