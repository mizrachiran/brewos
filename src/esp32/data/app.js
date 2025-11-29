/**
 * ECM Controller - Web UI Application
 */

// State
let ws = null;
let wsConnected = false;
let logPaused = false;
let picoConnected = false;

// Machine state names
const STATE_NAMES = [
    'INIT', 'IDLE', 'HEATING', 'READY', 
    'BREWING', 'STEAMING', 'COOLDOWN', 'FAULT', 'SAFE'
];

// Elements
const elements = {
    wsStatus: document.getElementById('ws-status'),
    wsText: document.getElementById('ws-text'),
    wifiSetup: document.getElementById('wifi-setup'),
    wifiSsid: document.getElementById('wifi-ssid'),
    wifiPassword: document.getElementById('wifi-password'),
    wifiStatus: document.getElementById('wifi-status'),
    brewTemp: document.getElementById('brew-temp'),
    steamTemp: document.getElementById('steam-temp'),
    brewSp: document.getElementById('brew-sp'),
    steamSp: document.getElementById('steam-sp'),
    brewOutput: document.getElementById('brew-output'),
    steamOutput: document.getElementById('steam-output'),
    pressure: document.getElementById('pressure'),
    waterLevel: document.getElementById('water-level'),
    waterPct: document.getElementById('water-pct'),
    picoStatus: document.getElementById('pico-status'),
    picoPackets: document.getElementById('pico-packets'),
    picoErrors: document.getElementById('pico-errors'),
    picoState: document.getElementById('pico-state'),
    picoUptime: document.getElementById('pico-uptime'),
    logContainer: document.getElementById('log-container'),
    otaFile: document.getElementById('ota-file'),
    uploadBtn: document.getElementById('upload-btn'),
    flashBtn: document.getElementById('flash-btn'),
    otaProgress: document.getElementById('ota-progress'),
    otaStatus: document.getElementById('ota-status'),
    espVersion: document.getElementById('esp-version'),
    espHeap: document.getElementById('esp-heap'),
    espUptime: document.getElementById('esp-uptime')
};

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    initWebSocket();
    initButtons();
    initMQTT();
    fetchStatus();
    fetchMQTTConfig();
    
    // Periodic status fetch
    setInterval(fetchStatus, 5000);
});

// WebSocket
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    log('Connecting to WebSocket...', 'info');
    elements.wsStatus.className = 'status-dot connecting';
    elements.wsText.textContent = 'Connecting...';
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        wsConnected = true;
        elements.wsStatus.className = 'status-dot connected';
        elements.wsText.textContent = 'Connected';
        log('WebSocket connected', 'info');
    };
    
    ws.onclose = () => {
        wsConnected = false;
        elements.wsStatus.className = 'status-dot disconnected';
        elements.wsText.textContent = 'Disconnected';
        log('WebSocket disconnected', 'warn');
        
        // Reconnect after delay
        setTimeout(initWebSocket, 3000);
    };
    
    ws.onerror = (error) => {
        log('WebSocket error', 'error');
    };
    
    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            handleMessage(data);
        } catch (e) {
            log('Invalid message: ' + event.data, 'error');
        }
    };
}

function handleMessage(data) {
    switch (data.type) {
        case 'esp_status':
            updateEspStatus(data);
            break;
            
        case 'pico':
            handlePicoMessage(data);
            break;
            
        case 'log':
            log(data.message, data.level);
            break;
            
        case 'ota_progress':
            handleOTAProgress(data);
            break;
            
        default:
            console.log('Unknown message:', data);
    }
}

function updateEspStatus(data) {
    picoConnected = data.picoConnected;
    
    elements.picoStatus.className = `status-badge ${picoConnected ? 'connected' : 'disconnected'}`;
    elements.picoStatus.textContent = picoConnected ? 'Connected' : 'Disconnected';
    elements.picoPackets.textContent = data.picoPackets || 0;
    elements.picoErrors.textContent = data.picoErrors || 0;
    
    elements.espHeap.textContent = formatBytes(data.heap);
    elements.espUptime.textContent = formatUptime(data.uptime);
}

function handlePicoMessage(data) {
    log(`Pico [0x${data.msgType.toString(16).padStart(2, '0')}] len=${data.length}`, 'pico');
    
    // Parse status payload (type 0x01)
    if (data.msgType === 0x01 && data.payload) {
        parseStatusPayload(data.payload);
    }
}

function parseStatusPayload(hexPayload) {
    // Convert hex string to bytes
    const bytes = [];
    for (let i = 0; i < hexPayload.length; i += 2) {
        bytes.push(parseInt(hexPayload.substr(i, 2), 16));
    }
    
    if (bytes.length < 24) return;
    
    // Parse status_payload_t (packed struct)
    const view = new DataView(new Uint8Array(bytes).buffer);
    
    const brewTemp = view.getInt16(0, true) / 10;
    const steamTemp = view.getInt16(2, true) / 10;
    const groupTemp = view.getInt16(4, true) / 10;
    const pressure = view.getUint16(6, true) / 100;
    const brewSp = view.getInt16(8, true) / 10;
    const steamSp = view.getInt16(10, true) / 10;
    const brewOutput = bytes[12];
    const steamOutput = bytes[13];
    const pumpOutput = bytes[14];
    const state = bytes[15];
    const flags = bytes[16];
    const waterLevel = bytes[17];
    const powerWatts = view.getUint16(18, true);
    const uptime = view.getUint32(20, true);
    
    // Update UI
    elements.brewTemp.textContent = brewTemp.toFixed(1);
    elements.steamTemp.textContent = steamTemp.toFixed(1);
    elements.brewSp.textContent = `${brewSp.toFixed(1)}°C`;
    elements.steamSp.textContent = `${steamSp.toFixed(1)}°C`;
    elements.brewOutput.style.width = `${brewOutput}%`;
    elements.steamOutput.style.width = `${steamOutput}%`;
    elements.pressure.textContent = pressure.toFixed(2);
    elements.waterLevel.style.height = `${waterLevel}%`;
    elements.waterPct.textContent = `${waterLevel}%`;
    elements.picoState.textContent = STATE_NAMES[state] || `${state}`;
    elements.picoUptime.textContent = formatUptime(uptime);
}

// Button handlers
function initButtons() {
    // WiFi
    document.getElementById('scan-btn').onclick = scanWifiNetworks;
    document.getElementById('connect-btn').onclick = connectWifi;
    
    // Pico
    document.getElementById('ping-btn').onclick = () => sendCommand('ping');
    document.getElementById('reset-btn').onclick = resetPico;
    
    // OTA
    elements.otaFile.onchange = () => {
        elements.uploadBtn.disabled = !elements.otaFile.files.length;
    };
    elements.uploadBtn.onclick = uploadFirmware;
    elements.flashBtn.onclick = flashPico;
    
    // Log
    document.getElementById('log-clear').onclick = clearLog;
    document.getElementById('log-pause').onclick = toggleLogPause;
}

async function fetchStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        
        elements.espVersion.textContent = data.esp32?.version || '--';
        elements.espHeap.textContent = formatBytes(data.esp32?.freeHeap || 0);
        elements.espUptime.textContent = formatUptime(data.esp32?.uptime || 0);
        
        // Check if in AP mode
        if (data.wifi?.mode === 1) {  // AP_MODE
            elements.wifiSetup.classList.remove('hidden');
        } else {
            elements.wifiSetup.classList.add('hidden');
        }
        
        // Update MQTT status
        if (data.mqtt) {
            const statusEl = document.getElementById('mqtt-status');
            if (!data.mqtt.enabled) {
                statusEl.textContent = 'Disabled';
                statusEl.className = 'status-badge disconnected';
            } else if (data.mqtt.connected) {
                statusEl.textContent = 'Connected';
                statusEl.className = 'status-badge connected';
            } else {
                statusEl.textContent = data.mqtt.status || 'Disconnected';
                statusEl.className = 'status-badge disconnected';
            }
        }
    } catch (e) {
        console.error('Status fetch failed:', e);
    }
}

async function scanWifiNetworks() {
    const btn = document.getElementById('scan-btn');
    btn.disabled = true;
    btn.textContent = 'Scanning...';
    
    try {
        const response = await fetch('/api/wifi/networks');
        const data = await response.json();
        
        elements.wifiSsid.innerHTML = '<option value="">Select network...</option>';
        data.networks.forEach(net => {
            const opt = document.createElement('option');
            opt.value = net.ssid;
            opt.textContent = `${net.ssid} (${net.rssi} dBm)${net.secure ? ' 🔒' : ''}`;
            elements.wifiSsid.appendChild(opt);
        });
    } catch (e) {
        log('WiFi scan failed', 'error');
    }
    
    btn.disabled = false;
    btn.textContent = 'Scan Networks';
}

async function connectWifi() {
    const ssid = elements.wifiSsid.value;
    const password = elements.wifiPassword.value;
    
    if (!ssid || !password) {
        elements.wifiStatus.textContent = 'Please select a network and enter password';
        return;
    }
    
    elements.wifiStatus.textContent = 'Connecting...';
    
    try {
        const response = await fetch('/api/wifi/connect', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ssid, password })
        });
        
        const data = await response.json();
        elements.wifiStatus.textContent = data.message || 'Connecting...';
        
        // Page will reload when connected to new network
        setTimeout(() => {
            elements.wifiStatus.textContent = 'If connected, page will reload with new IP...';
        }, 5000);
    } catch (e) {
        elements.wifiStatus.textContent = 'Connection failed';
    }
}

function sendCommand(cmd) {
    if (ws && wsConnected) {
        ws.send(JSON.stringify({ type: cmd }));
    }
}

async function resetPico() {
    if (!confirm('Reset the Pico controller?')) return;
    
    try {
        await fetch('/api/pico/reset', { method: 'POST' });
        log('Pico reset triggered', 'warn');
    } catch (e) {
        log('Reset failed', 'error');
    }
}

async function uploadFirmware() {
    const file = elements.otaFile.files[0];
    if (!file) return;
    
    const formData = new FormData();
    formData.append('firmware', file);
    
    elements.uploadBtn.disabled = true;
    elements.flashBtn.disabled = true;
    document.querySelector('.ota-progress').classList.remove('hidden');
    elements.otaProgress.style.width = '0%';
    elements.otaStatus.textContent = 'Uploading...';
    
    try {
        const response = await fetch('/api/ota/upload', {
            method: 'POST',
            body: formData
        });
        
        if (response.ok) {
            // Progress will be updated via WebSocket
            // Wait a bit for final progress update
            await new Promise(resolve => setTimeout(resolve, 500));
            
            if (elements.otaProgress.style.width === '100%') {
                elements.otaStatus.textContent = 'Upload complete';
                elements.flashBtn.disabled = false;
                log('Firmware uploaded successfully', 'info');
            } else {
                elements.otaStatus.textContent = 'Upload complete';
                elements.flashBtn.disabled = false;
                log('Firmware uploaded successfully', 'info');
            }
        } else {
            throw new Error('Upload failed');
        }
    } catch (e) {
        elements.otaStatus.textContent = 'Upload failed';
        elements.otaProgress.style.width = '0%';
        log('Firmware upload failed', 'error');
    }
    
    elements.uploadBtn.disabled = false;
}

async function flashPico() {
    if (!confirm('Flash the Pico with uploaded firmware? This will reset the Pico.')) return;
    
    elements.flashBtn.disabled = true;
    elements.uploadBtn.disabled = true;
    elements.otaProgress.style.width = '0%';
    elements.otaStatus.textContent = 'Starting flash...';
    document.querySelector('.ota-progress').classList.remove('hidden');
    
    try {
        const response = await fetch('/api/ota/start', { method: 'POST' });
        const data = await response.json();
        if (data.error) {
            elements.otaStatus.textContent = 'Error: ' + data.error;
            log('Flash error: ' + data.error, 'error');
            elements.flashBtn.disabled = false;
            return;
        }
        elements.otaStatus.textContent = data.message || 'Flashing...';
        // Progress will be updated via WebSocket
    } catch (e) {
        elements.otaStatus.textContent = 'Flash failed';
        log('Flash request failed', 'error');
        elements.flashBtn.disabled = false;
    }
}

// Logging
function log(message, level = 'info') {
    if (logPaused) return;
    
    const entry = document.createElement('div');
    entry.className = `log-entry ${level}`;
    entry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
    
    elements.logContainer.appendChild(entry);
    elements.logContainer.scrollTop = elements.logContainer.scrollHeight;
    
    // Limit log entries
    while (elements.logContainer.children.length > 200) {
        elements.logContainer.removeChild(elements.logContainer.firstChild);
    }
}

function clearLog() {
    elements.logContainer.innerHTML = '';
    log('Log cleared', 'info');
}

function toggleLogPause() {
    logPaused = !logPaused;
    document.getElementById('log-pause').textContent = logPaused ? 'Resume' : 'Pause';
}

function handleOTAProgress(data) {
    const stage = data.stage || 'upload';
    const progress = data.progress || 0;
    
    elements.otaProgress.style.width = progress + '%';
    
    if (stage === 'upload') {
        const uploaded = data.uploaded || 0;
        const total = data.total || 0;
        if (total > 0) {
            elements.otaStatus.textContent = `Uploading: ${progress}% (${formatBytes(uploaded)}/${formatBytes(total)})`;
        } else {
            elements.otaStatus.textContent = `Uploading: ${progress}%`;
        }
    } else if (stage === 'flash') {
        const sent = data.sent || 0;
        const total = data.total || 0;
        if (total > 0) {
            elements.otaStatus.textContent = `Flashing: ${progress}% (${formatBytes(sent)}/${formatBytes(total)})`;
        } else {
            elements.otaStatus.textContent = `Flashing: ${progress}%`;
        }
        
        if (progress >= 100) {
            elements.otaStatus.textContent = 'Flash complete! Pico will reset...';
            // Re-enable buttons after a delay
            setTimeout(() => {
                elements.uploadBtn.disabled = false;
                elements.flashBtn.disabled = false;
            }, 3000);
        }
    }
}

// Utilities
function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    
    if (hours > 0) {
        return `${hours}h ${minutes % 60}m`;
    } else if (minutes > 0) {
        return `${minutes}m ${seconds % 60}s`;
    } else {
        return `${seconds}s`;
    }
}

// MQTT Configuration
function initMQTT() {
    document.getElementById('mqtt-save-btn').addEventListener('click', saveMQTTConfig);
    document.getElementById('mqtt-test-btn').addEventListener('click', testMQTT);
}

async function fetchMQTTConfig() {
    try {
        const response = await fetch('/api/mqtt/config');
        const config = await response.json();
        
        document.getElementById('mqtt-enabled').checked = config.enabled || false;
        document.getElementById('mqtt-broker').value = config.broker || '';
        document.getElementById('mqtt-port').value = config.port || 1883;
        document.getElementById('mqtt-username').value = config.username || '';
        document.getElementById('mqtt-password').value = '';  // Don't show password
        document.getElementById('mqtt-topic-prefix').value = config.topic_prefix || 'brewos';
        document.getElementById('mqtt-ha-discovery').checked = config.ha_discovery !== false;
        document.getElementById('mqtt-device-id').value = config.ha_device_id || '';
        
        updateMQTTStatus(config);
    } catch (error) {
        log('Failed to fetch MQTT config: ' + error, 'error');
    }
}

function updateMQTTStatus(config) {
    const statusEl = document.getElementById('mqtt-status');
    if (!config.enabled) {
        statusEl.textContent = 'Disabled';
        statusEl.className = 'status-badge disconnected';
    } else if (config.connected) {
        statusEl.textContent = 'Connected';
        statusEl.className = 'status-badge connected';
    } else {
        statusEl.textContent = config.status || 'Disconnected';
        statusEl.className = 'status-badge disconnected';
    }
}

async function saveMQTTConfig() {
    const messageEl = document.getElementById('mqtt-message');
    messageEl.textContent = 'Saving...';
    messageEl.className = 'mqtt-message info';
    
    const config = {
        enabled: document.getElementById('mqtt-enabled').checked,
        broker: document.getElementById('mqtt-broker').value.trim(),
        port: parseInt(document.getElementById('mqtt-port').value) || 1883,
        username: document.getElementById('mqtt-username').value.trim(),
        password: document.getElementById('mqtt-password').value,
        topic_prefix: document.getElementById('mqtt-topic-prefix').value.trim() || 'brewos',
        ha_discovery: document.getElementById('mqtt-ha-discovery').checked,
        ha_device_id: document.getElementById('mqtt-device-id').value.trim()
    };
    
    // Validate
    if (config.enabled && !config.broker) {
        messageEl.textContent = 'Error: Broker address is required';
        messageEl.className = 'mqtt-message error';
        return;
    }
    
    try {
        const response = await fetch('/api/mqtt/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        const result = await response.json();
        
        if (response.ok) {
            messageEl.textContent = 'Configuration saved successfully';
            messageEl.className = 'mqtt-message success';
            log('MQTT configuration saved', 'info');
            
            // Refresh config to get updated status
            setTimeout(fetchMQTTConfig, 1000);
        } else {
            messageEl.textContent = 'Error: ' + (result.error || 'Failed to save');
            messageEl.className = 'mqtt-message error';
            log('Failed to save MQTT config: ' + (result.error || 'Unknown error'), 'error');
        }
    } catch (error) {
        messageEl.textContent = 'Error: ' + error.message;
        messageEl.className = 'mqtt-message error';
        log('Failed to save MQTT config: ' + error, 'error');
    }
}

async function testMQTT() {
    const messageEl = document.getElementById('mqtt-message');
    messageEl.textContent = 'Testing connection...';
    messageEl.className = 'mqtt-message info';
    
    try {
        const response = await fetch('/api/mqtt/test', {
            method: 'POST'
        });
        
        const result = await response.json();
        
        if (response.ok) {
            messageEl.textContent = result.message || 'Connection successful!';
            messageEl.className = 'mqtt-message success';
            log('MQTT connection test successful', 'info');
        } else {
            messageEl.textContent = 'Error: ' + (result.error || 'Connection failed');
            messageEl.className = 'mqtt-message error';
            log('MQTT connection test failed: ' + (result.error || 'Unknown error'), 'error');
        }
    } catch (error) {
        messageEl.textContent = 'Error: ' + error.message;
        messageEl.className = 'mqtt-message error';
        log('Failed to test MQTT connection: ' + error, 'error');
    }
}

