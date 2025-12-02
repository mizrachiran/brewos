# API Versioning & Feature Detection

## Overview

BrewOS uses a multi-layered versioning system to ensure compatibility between the web UI, ESP32 firmware, and cloud server:

1. **API Version** - Breaking changes to REST/WebSocket APIs
2. **Feature Flags** - Granular capability detection
3. **Firmware Version** - Semantic versioning for releases
4. **Protocol Version** - Pico↔ESP32 binary protocol (separate concern)

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         VERSION ARCHITECTURE                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Web UI                         ESP32 / Cloud                       │
│  ┌────────────────────┐        ┌────────────────────┐              │
│  │                    │        │                    │              │
│  │  fetch /api/info   │───────▶│  apiVersion: 1     │              │
│  │                    │        │  features: [...]   │              │
│  │  Check:            │◀───────│  firmwareVersion   │              │
│  │  - apiVersion >= 1 │        │  mode: local/cloud │              │
│  │  - hasFeature(x)   │        │                    │              │
│  │                    │        └────────────────────┘              │
│  │  Conditionally     │                                            │
│  │  render features   │                                            │
│  │                    │                                            │
│  └────────────────────┘                                            │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## The `/api/info` Endpoint

Both ESP32 and Cloud server provide this endpoint:

### Request

```http
GET /api/info
```

### Response

```json
{
  "apiVersion": 1,
  "firmwareVersion": "0.2.0",
  "webVersion": "0.2.0",
  "protocolVersion": 1,
  "picoConnected": true,
  "mode": "local",
  "apMode": false,
  "features": [
    "temperature_control",
    "pressure_monitoring",
    "power_monitoring",
    "bbw",
    "scale",
    "mqtt",
    "eco_mode",
    "statistics",
    "schedules",
    "pico_ota",
    "esp32_ota",
    "debug_console",
    "protocol_debug"
  ],
  "deviceId": "AA:BB:CC:DD:EE:FF",
  "hostname": "brewos"
}
```

### Cloud-Only Fields

```json
{
  "features": ["push_notifications", "remote_access", "multi_device"],
  "connectedDevices": 3,
  "connectedClients": 5
}
```

## API Version Rules

| Change Type                    | Action                 | Example                        |
| ------------------------------ | ---------------------- | ------------------------------ |
| Breaking REST/WebSocket change | Increment `apiVersion` | Changing message format        |
| New REST endpoint              | Add feature flag       | `features: ["new_feature"]`    |
| New WebSocket message type     | Add feature flag       | `features: ["new_capability"]` |
| Removed endpoint/message       | Increment `apiVersion` | Removing deprecated API        |
| Bug fix                        | No change              | Internal fix                   |

### Breaking Changes (Require API Version Bump)

- Changing the structure of existing WebSocket messages
- Removing or renaming REST endpoints
- Changing authentication requirements
- Modifying request/response payload schemas

### Non-Breaking Changes (Add Feature Flag)

- Adding new REST endpoints
- Adding new WebSocket message types
- Adding optional fields to existing messages
- Adding new configuration options

## Feature Flags

### Core Features (Always Available)

| Feature               | Description                      |
| --------------------- | -------------------------------- |
| `temperature_control` | Temperature setpoint control     |
| `pressure_monitoring` | Pressure sensor reading          |
| `power_monitoring`    | Power/voltage/current monitoring |

### Advanced Features

| Feature      | Description                  |
| ------------ | ---------------------------- |
| `bbw`        | Brew-by-weight functionality |
| `scale`      | BLE scale integration        |
| `mqtt`       | MQTT broker integration      |
| `eco_mode`   | Eco/sleep mode               |
| `statistics` | Shot statistics tracking     |
| `schedules`  | Schedule management          |

### OTA Features

| Feature     | Description                |
| ----------- | -------------------------- |
| `pico_ota`  | Pico firmware OTA updates  |
| `esp32_ota` | ESP32 firmware OTA updates |

### Cloud-Only Features

| Feature              | Description                |
| -------------------- | -------------------------- |
| `push_notifications` | Push notification support  |
| `remote_access`      | Remote device access       |
| `multi_device`       | Multiple device management |

### Debug Features

| Feature          | Description              |
| ---------------- | ------------------------ |
| `debug_console`  | Debug console access     |
| `protocol_debug` | Protocol debugging tools |

## Web UI Implementation

### Feature Detection

```typescript
import { useFeature, FEATURES } from "@/lib/backend-info";

function MyComponent() {
  const hasBBW = useFeature(FEATURES.BREW_BY_WEIGHT);

  if (!hasBBW) {
    return null; // Hide if not supported
  }

  return <BrewByWeightCard />;
}
```

### Feature Gate Component

```tsx
import { FeatureGate, FEATURES } from "@/components/FeatureGate";

function SettingsPage() {
  return (
    <div>
      <TemperatureSettings />

      <FeatureGate feature={FEATURES.ECO_MODE}>
        <EcoModeSettings />
      </FeatureGate>

      <FeatureGate feature={FEATURES.PUSH_NOTIFICATIONS} showUnavailable>
        <NotificationSettings />
      </FeatureGate>
    </div>
  );
}
```

### Version Compatibility Check

```typescript
import { useBackendInfo } from "@/lib/backend-info";

function App() {
  const { compatible, warnings, errors } = useBackendInfo();

  if (!compatible) {
    return <FirmwareUpdateRequired errors={errors} />;
  }

  return <MainApp />;
}
```

## Adding New Features

### 1. Add Feature Flag to ESP32

```cpp
// In web_server.cpp, setupRoutes(), /api/info handler
features.add("my_new_feature");
```

### 2. Add Feature Flag to Cloud

```typescript
// In server.ts, /api/info handler
features: [
  // existing...
  "my_new_feature",
];
```

### 3. Add Feature Constant to Web UI

```typescript
// In src/web/src/lib/api-version.ts
export const FEATURES = {
  // existing...
  MY_NEW_FEATURE: "my_new_feature",
} as const;
```

### 4. Gate the Feature in UI

```tsx
<FeatureGate feature={FEATURES.MY_NEW_FEATURE}>
  <MyNewFeatureComponent />
</FeatureGate>
```

## Version History

| API Version | Firmware | Changes         |
| ----------- | -------- | --------------- |
| 1           | 0.1.0+   | Initial release |

## Best Practices

1. **Always check features** - Don't assume features exist
2. **Graceful degradation** - Hide unavailable features, don't crash
3. **User feedback** - Show "Feature unavailable" messages when appropriate
4. **Version warnings** - Alert users when firmware is outdated
5. **Test both modes** - Test local (ESP32) and cloud modes
6. **Document breaking changes** - Update this file when incrementing API version

## Troubleshooting

### Feature Not Showing

1. Check `/api/info` response includes the feature
2. Verify `useFeature()` hook is used correctly
3. Check browser console for errors

### Version Mismatch Warning

1. Update device firmware to latest version
2. Clear browser cache and reload
3. Check if cloud server is up to date

### API Calls Failing

1. Check API version compatibility
2. Verify feature flag is present
3. Check network connectivity
