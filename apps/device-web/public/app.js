const el = (id) => document.getElementById(id);

const deviceStatus = el('deviceStatus');
const wifiStatus = el('wifiStatus');
const audioStatus = el('audioStatus');
const apStatus = el('apStatus');
const audioDetails = el('audioDetails');
const devicePill = el('devicePill');
const wifiPill = el('wifiPill');
const deviceLoading = el('deviceLoading');
const wifiLoading = el('wifiLoading');
const audioLoading = el('audioLoading');
const scanLoading = el('scanLoading');
const connectLoading = el('connectLoading');
const apLoading = el('apLoading');
const audioSaveLoading = el('audioSaveLoading');
const ssidList = el('ssidList');
const ssidInput = el('ssidInput');
const passwordInput = el('passwordInput');
const apEnabled = el('apEnabled');
const apSsid = el('apSsid');
const audioMode = el('audioMode');
const audioUrl = el('audioUrl');
const serverTarget = el('serverTarget');

const statusError = el('statusError');
const wifiStatusError = el('wifiStatusError');
const scanError = el('scanError');
const connectError = el('connectError');
const apError = el('apError');
const audioError = el('audioError');
const audioFormError = el('audioFormError');

async function api(path, options = {}) {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...options,
  });
  if (!res.ok) {
    const text = await res.text();
    throw new Error(text || `Request failed: ${res.status}`);
  }
  const contentType = res.headers.get('content-type') || '';
  if (contentType.includes('application/json')) {
    return res.json();
  }
  return res.text();
}

function setError(elm, err) {
  elm.textContent = err ? err.message : '';
}

function setLoading(elm, loading) {
  if (!elm) return;
  elm.classList.toggle('hidden', !loading);
}

function renderStatusGrid(elm, items) {
  elm.innerHTML = '';
  items.forEach(({ label, value }) => {
    const row = document.createElement('div');
    row.className = 'status-row';
    row.innerHTML = `<span>${label}</span><strong>${value}</strong>`;
    elm.appendChild(row);
  });
}

function setPill(elm, ok, okText, warnText) {
  elm.textContent = ok ? okText : warnText;
  elm.className = ok ? 'pill' : 'pill warn';
}

async function loadConfig() {
  setError(statusError, null);
  setLoading(deviceLoading, true);
  try {
    const data = await api('/api/v1/config');
    renderStatusGrid(deviceStatus, [
      { label: 'Device Name', value: data.deviceName || 'Unknown' },
      { label: 'Setup Done', value: data.isSetupDone ? 'Yes' : 'No' },
      { label: 'Wi‑Fi Connected', value: data.wifiConnected ? 'Yes' : 'No' },
      { label: 'Wi‑Fi Configured', value: data.wifiConfigured ? 'Yes' : 'No' },
      { label: 'AP Enabled', value: data.apEnabled ? 'Yes' : 'No' },
      { label: 'Audio Configured', value: data.audioConfigured ? 'Yes' : 'No' },
    ]);
    setPill(devicePill, data.isSetupDone, 'Ready', 'Needs setup');
  } catch (err) {
    setError(statusError, err);
  } finally {
    setLoading(deviceLoading, false);
  }
}

async function loadWifiStatus() {
  setError(wifiStatusError, null);
  setLoading(wifiLoading, true);
  try {
    const data = await api('/api/v1/wifi/status');
    renderStatusGrid(wifiStatus, [
      { label: 'Connected', value: data.connected ? 'Yes' : 'No' },
      { label: 'Configured', value: data.configured ? 'Yes' : 'No' },
      { label: 'SSID', value: data.ssid || '—' },
      { label: 'AP Enabled', value: data.apEnabled ? 'Yes' : 'No' },
      { label: 'AP SSID', value: data.apSsid || '—' },
    ]);
    apEnabled.value = String(data.apEnabled);
    apSsid.value = data.apSsid || '';
    ssidInput.value = data.ssid || '';
    renderStatusGrid(apStatus, [
      { label: 'AP Enabled', value: data.apEnabled ? 'Yes' : 'No' },
      { label: 'AP SSID', value: data.apSsid || '—' },
    ]);
    setPill(wifiPill, data.connected, 'Connected', 'Offline');
  } catch (err) {
    setError(wifiStatusError, err);
  } finally {
    setLoading(wifiLoading, false);
  }
}

async function scanWifi() {
  setError(scanError, null);
  ssidList.innerHTML = '';
  setLoading(scanLoading, true);
  try {
    const data = await api('/api/v1/wifi/scan');
    data.ssids.forEach((ssid) => {
      const li = document.createElement('li');
      li.textContent = ssid;
      li.addEventListener('click', () => {
        ssidInput.value = ssid;
      });
      ssidList.appendChild(li);
    });
  } catch (err) {
    setError(scanError, err);
  } finally {
    setLoading(scanLoading, false);
  }
}

async function connectWifi() {
  setError(connectError, null);
  setLoading(connectLoading, true);
  try {
    await api('/api/v1/wifi/connect', {
      method: 'POST',
      body: JSON.stringify({
        ssid: ssidInput.value.trim(),
        password: passwordInput.value,
      }),
    });
    await loadWifiStatus();
  } catch (err) {
    setError(connectError, err);
  } finally {
    setLoading(connectLoading, false);
  }
}

async function saveAp() {
  setError(apError, null);
  setLoading(apLoading, true);
  try {
    await api('/api/v1/wifi/ap', {
      method: 'POST',
      body: JSON.stringify({
        enabled: apEnabled.value === 'true',
        ssid: apSsid.value.trim(),
      }),
    });
    await loadWifiStatus();
  } catch (err) {
    setError(apError, err);
  } finally {
    setLoading(apLoading, false);
  }
}

async function loadAudio() {
  setError(audioError, null);
  setLoading(audioLoading, true);
  try {
    const data = await api('/api/v1/audio');
    audioMode.value = data.mode || '';
    audioUrl.value = data.uploadUrl || '';
    renderStatusGrid(audioStatus, [
      { label: 'Mode', value: data.mode || '—' },
      { label: 'Upload URL', value: data.uploadUrl || '—' },
    ]);
    renderStatusGrid(audioDetails, [
      { label: 'Mode', value: data.mode || '—' },
      { label: 'Upload URL', value: data.uploadUrl || '—' },
    ]);
  } catch (err) {
    setError(audioError, err);
  } finally {
    setLoading(audioLoading, false);
  }
}

async function saveAudio() {
  setError(audioFormError, null);
  setLoading(audioSaveLoading, true);
  try {
    await api('/api/v1/audio', {
      method: 'POST',
      body: JSON.stringify({
        mode: audioMode.value.trim(),
        uploadUrl: audioUrl.value.trim(),
      }),
    });
    await loadAudio();
  } catch (err) {
    setError(audioFormError, err);
  } finally {
    setLoading(audioSaveLoading, false);
  }
}

async function loadServerTarget() {
  try {
    const data = await api('/config');
    serverTarget.textContent = data.target;
  } catch (err) {
    serverTarget.textContent = 'Unknown';
  }
}

el('refreshStatus').addEventListener('click', loadConfig);
el('refreshWifi').addEventListener('click', loadWifiStatus);
el('refreshAudio').addEventListener('click', loadAudio);
el('scanWifi').addEventListener('click', scanWifi);
el('connectWifi').addEventListener('click', connectWifi);
el('saveAp').addEventListener('click', saveAp);
el('saveAudio').addEventListener('click', saveAudio);
el('refreshAll').addEventListener('click', async () => {
  await loadConfig();
  await loadWifiStatus();
  await loadAudio();
});

loadConfig();
loadWifiStatus();
loadAudio();
loadServerTarget();

document.querySelectorAll('.tab').forEach((tab) => {
  tab.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach((t) => t.classList.remove('active'));
    document.querySelectorAll('.panel').forEach((panel) => panel.classList.remove('active'));
    tab.classList.add('active');
    const target = document.getElementById(tab.dataset.tab);
    if (target) {
      target.classList.add('active');
    }
  });
});
