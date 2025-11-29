# Shot Timer Display Implementation Guide

## Overview

The shot timer is already implemented in the Pico firmware and sent in the status payload. This guide shows how to display it on the ESP32 web UI.

## Data Flow

1. **Pico** → Sends `MSG_STATUS` with `status_payload_t` (includes `shot_timer_ms`)
2. **ESP32** → Receives via UART, forwards to WebSocket clients
3. **Web UI** → Parses payload and updates display

## Implementation Steps

### Step 1: Update Status Payload Parser

The `parseStatusPayload()` function in `src/esp32/data/app.js` needs to read the new `shot_start_timestamp_ms` field and calculate elapsed time.

**Current structure (with shot_start_timestamp_ms):**
- Offset 0-19: Existing fields (brew_temp, steam_temp, etc.)
- Offset 20-23: `uptime_ms` (uint32_t, 4 bytes)
- Offset 24-27: **NEW** `shot_start_timestamp_ms` (uint32_t, 4 bytes) - Brew start timestamp (0 if not brewing)

**Update `parseStatusPayload()` in `src/esp32/data/app.js`:**

```javascript
function parseStatusPayload(hexPayload) {
    // Convert hex string to bytes
    const bytes = [];
    for (let i = 0; i < hexPayload.length; i += 2) {
        bytes.push(parseInt(hexPayload.substr(i, 2), 16));
    }
    
    // Update minimum length check (was 24, now 28 with shot_start_timestamp_ms)
    if (bytes.length < 28) return;
    
    // Parse status_payload_t (packed struct)
    const view = new DataView(new Uint8Array(bytes).buffer);
    
    // Existing fields (offsets 0-19)
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
    
    // NEW: Read shot start timestamp (offset 24, uint32_t, little-endian)
    const shotStartTimestamp = view.getUint32(24, true);
    
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
    
    // NEW: Calculate and display shot timer
    if (shotStartTimestamp > 0) {
        // Calculate elapsed time using client clock
        const elapsed = Date.now() - (uptime - shotStartTimestamp);
        updateShotTimerDisplay(elapsed);
    } else {
        elements.shotTimer.textContent = '--:--';
    }
}
```

### Step 2: Add UI Element

Add a shot timer display card in `src/esp32/data/index.html`:

```html
<!-- Add this card in the status-grid section, after the Pressure card -->
<div class="card">
    <div class="card-header">
        <span class="icon">⏱️</span>
        <span>Shot Timer</span>
    </div>
    <div class="shot-timer-display">
        <span class="timer-value" id="shot-timer">--:--</span>
    </div>
</div>
```

### Step 3: Add Element Reference

Add the shot timer element to the `elements` object in `src/esp32/data/app.js`:

```javascript
const elements = {
    // ... existing elements ...
    picoUptime: document.getElementById('pico-uptime'),
    shotTimer: document.getElementById('shot-timer'),  // NEW
    logContainer: document.getElementById('log-container'),
    // ... rest of elements ...
};
```

### Step 4: Add CSS Styling (Optional)

Add styling in `src/esp32/data/style.css` for the shot timer display:

```css
.shot-timer-display {
    text-align: center;
    padding: 20px;
}

.timer-value {
    font-size: 2.5em;
    font-weight: bold;
    font-family: 'Courier New', monospace;
    color: #2c3e50;
}

/* Highlight when brewing */
.card:has(#shot-timer:not(:contains('--'))) {
    border-color: #3498db;
}
```

## Format Options

The shot timer can be displayed in different formats:

### Format 1: Seconds with decimals (e.g., "25.43s")
```javascript
const seconds = shotTimerMs / 1000;
elements.shotTimer.textContent = `${seconds.toFixed(2)}s`;
```

### Format 2: MM:SS format (e.g., "0:25")
```javascript
const totalSeconds = Math.floor(shotTimerMs / 1000);
const minutes = Math.floor(totalSeconds / 60);
const seconds = totalSeconds % 60;
elements.shotTimer.textContent = `${minutes}:${String(seconds).padStart(2, '0')}`;
```

### Format 3: SS.mm format (e.g., "25.43")
```javascript
const seconds = Math.floor(shotTimerMs / 1000);
const milliseconds = shotTimerMs % 1000;
elements.shotTimer.textContent = `${seconds}.${String(Math.floor(milliseconds / 10)).padStart(2, '0')}`;
```

## Smooth Timer Implementation

Since status messages are sent every 250ms, the timer would update in jumps. To make it smooth, implement a **client-side timer** that runs locally at high frequency and syncs with server time.

### How It Works

1. **Server sends timer value** every 250ms in status message
2. **Client starts local timer** when brewing begins
3. **Client updates display** every 10-50ms (smooth)
4. **Client syncs with server** on each status message (corrects drift)

### Complete Implementation

Add this to `src/esp32/data/app.js`:

**Step 1: Add timer state variables (at top of file, after other state variables):**

```javascript
// Shot timer state (for smooth client-side updates)
let shotTimerInterval = null;
let shotTimerStartTime = null;  // Client timestamp when brew started (Date.now())
let lastBrewingState = false;
let picoBootTime = null;  // Client timestamp when Pico booted (calculated from uptime)
```

**Step 2: Update parseStatusPayload() to handle smooth timer:**

```javascript
function parseStatusPayload(hexPayload) {
    // ... existing parsing code ...
    
    // Read shot start timestamp (offset 24, uint32_t, little-endian)
    const shotStartTimestamp = view.getUint32(24, true);
    const picoUptime = view.getUint32(20, true);  // Pico's uptime in ms
    
    // Calculate when Pico booted (relative to client clock)
    // This syncs on every message to correct for any clock drift
    picoBootTime = Date.now() - picoUptime;
    
    // Detect brewing state change
    const isBrewing = (state === 4); // STATE_BREWING = 4
    
    // Handle timer state transitions
    if (isBrewing && shotStartTimestamp > 0 && !lastBrewingState) {
        // Brew just started - convert Pico timestamp to client time
        shotTimerStartTime = picoBootTime + shotStartTimestamp;
        startSmoothTimer();
    } else if (!isBrewing && lastBrewingState) {
        // Brew just stopped - stop smooth timer, show final value
        stopSmoothTimer();
        if (shotStartTimestamp > 0 && shotTimerStartTime) {
            const finalElapsed = (picoBootTime + shotStartTimestamp) - shotTimerStartTime;
            updateShotTimerDisplay(finalElapsed);
        } else {
            updateShotTimerDisplay(0);
        }
    } else if (isBrewing && shotStartTimestamp > 0) {
        // Brew in progress - sync with server time to correct drift
        // Recalculate start time based on current Pico boot time
        shotTimerStartTime = picoBootTime + shotStartTimestamp;
    }
    
    lastBrewingState = isBrewing;
    
    // ... rest of UI updates ...
}
```

**Step 3: Add smooth timer functions:**

```javascript
/**
 * Start smooth client-side timer (updates every 10ms)
 */
function startSmoothTimer() {
    if (shotTimerInterval) return; // Already running
    
    shotTimerInterval = setInterval(() => {
        if (shotTimerStartTime) {
            const elapsed = Date.now() - shotTimerStartTime;
            updateShotTimerDisplay(elapsed);
        }
    }, 10); // Update every 10ms for smooth 100Hz display
}

/**
 * Stop smooth timer
 */
function stopSmoothTimer() {
    if (shotTimerInterval) {
        clearInterval(shotTimerInterval);
        shotTimerInterval = null;
    }
    shotTimerStartTime = null;
}

/**
 * Update shot timer display with formatted time
 * @param {number} ms - Time in milliseconds
 */
function updateShotTimerDisplay(ms) {
    if (!elements.shotTimer) return; // Element not found
    
    if (ms === 0 || ms === undefined) {
        elements.shotTimer.textContent = '--:--';
        return;
    }
    
    // Format as MM:SS.mm (e.g., "0:25.43" or "1:02.15")
    const totalSeconds = Math.floor(ms / 1000);
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    const centiseconds = Math.floor((ms % 1000) / 10); // 10ms resolution
    
    elements.shotTimer.textContent = 
        `${minutes}:${String(seconds).padStart(2, '0')}.${String(centiseconds).padStart(2, '0')}`;
}
```

**Step 4: Clean up on disconnect:**

```javascript
// In ws.onclose handler (around line 78):
ws.onclose = () => {
    wsConnected = false;
    elements.wsStatus.className = 'status-dot disconnected';
    elements.wsText.textContent = 'Disconnected';
    log('WebSocket disconnected', 'warn');
    
    // Stop smooth timer on disconnect
    stopSmoothTimer();
    lastBrewingState = false;
    
    // Reconnect after delay
    setTimeout(initWebSocket, 3000);
};
```

## How It Works

1. **Status messages arrive** every 250ms with `shot_timer_ms` value
2. **Client detects brewing start** (state changes to BREWING)
3. **Client starts local timer** that updates every 10ms (100Hz - very smooth)
4. **Client calculates start time** based on server's timer value
5. **Client syncs on each status message** to correct any clock drift
6. **Client stops timer** when brewing ends (state changes from BREWING)

### Benefits

- **Smooth display**: Updates every 10ms (100Hz) instead of every 250ms (4Hz)
- **Accurate**: Uses start timestamp, syncs with server time to prevent drift
- **Efficient**: Server sends timestamp (4 bytes) instead of calculating duration
- **Responsive**: Timer starts/stops immediately on state changes
- **Simple**: Client calculates elapsed time using its own clock

## Testing

1. Start a brew cycle (lever up or command)
2. Verify timer starts counting smoothly (updates every 10ms)
3. Stop brew (lever down or WEIGHT_STOP)
4. Verify timer stops and shows final time
5. Verify timer resets to 0 when not brewing

## Notes

- `shot_start_timestamp_ms` is 0 when not brewing
- Timestamp is relative to Pico boot time (milliseconds since boot)
- Client converts to absolute time using Pico uptime
- Status messages update every 250ms (from Pico)
- Client-side timer interpolates smoothly (updates every 10ms)
- Timer syncs with server time on each status message to correct drift
- Timer stops immediately on WEIGHT_STOP or lever down
- Timer shows elapsed time while brewing, final time when stopped

