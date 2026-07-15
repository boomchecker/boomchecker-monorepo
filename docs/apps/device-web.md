# Device web

`apps/device-web` is a minimal Node.js + Express UI for a device. It serves static
files from `public/` and proxies API requests to the device.

It plays a double role: run standalone for local development, **and** its `public/`
output is copied into `fw/bom-node/generated/` at firmware build time so the same UI
ships embedded on the ESP32 (see [`bom-node`](../firmware/bom-node.md)).

## Run locally

```bash
cd apps/device-web
task run        # node server.js
```

`task build` (invoked by the firmware build) copies `public/` into the firmware's
generated folder.

---

--8<-- "apps/device-web/README.md"
