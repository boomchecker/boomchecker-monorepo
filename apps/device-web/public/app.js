const el = (id) => document.getElementById(id);

const deviceStatus = el('deviceStatus');
const wifiStatus = el('wifiStatus');
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

async function loadConfig() {
  setError(statusError, null);
  try {
    const data = await api('/api/v1/config');
    deviceStatus.textContent = JSON.stringify(data, null, 2);
  } catch (err) {
    setError(statusError, err);
  }
}

async function loadWifiStatus() {
  setError(wifiStatusError, null);
  try {
    const data = await api('/api/v1/wifi/status');
    wifiStatus.textContent = JSON.stringify(data, null, 2);
    apEnabled.value = String(data.apEnabled);
    apSsid.value = data.apSsid || '';
    ssidInput.value = data.ssid || '';
  } catch (err) {
    setError(wifiStatusError, err);
  }
}

async function scanWifi() {
  setError(scanError, null);
  ssidList.innerHTML = '';
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
  }
}

async function connectWifi() {
  setError(connectError, null);
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
  }
}

async function saveAp() {
  setError(apError, null);
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
  }
}

async function loadAudio() {
  setError(audioError, null);
  try {
    const data = await api('/api/v1/audio');
    audioMode.value = data.mode || '';
    audioUrl.value = data.uploadUrl || '';
  } catch (err) {
    setError(audioError, err);
  }
}

async function saveAudio() {
  setError(audioError, null);
  try {
    await api('/api/v1/audio', {
      method: 'POST',
      body: JSON.stringify({
        mode: audioMode.value.trim(),
        uploadUrl: audioUrl.value.trim(),
      }),
    });
  } catch (err) {
    setError(audioError, err);
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
el('scanWifi').addEventListener('click', scanWifi);
el('connectWifi').addEventListener('click', connectWifi);
el('saveAp').addEventListener('click', saveAp);
el('saveAudio').addEventListener('click', saveAudio);

loadConfig();
loadWifiStatus();
loadAudio();
loadServerTarget();
