# Cloud Service Latency and Performance

## Overview

The BrewOS cloud service acts as a **WebSocket relay** between ESP32 devices and web clients. Network latency depends on the geographic location of:

1. The ESP32 device (your espresso machine)
2. The cloud VPS server
3. The web client (your browser/app)

---

## Network Path

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│   ESP32     │ ◄─────► │ Cloud VPS    │ ◄─────► │ Web Client  │
│ (Machine)   │         │ (Relay)      │         │ (Browser)   │
└─────────────┘         └──────────────┘         └─────────────┘
   Location A              Location B              Location C
```

**Total Round-Trip Time (RTT):**

```
RTT = (ESP32 → Cloud) + (Cloud → Client) + Processing Time
```

---

## Expected Latency by Geography

### Scenario 1: USA VPS, EU Machine, EU Client

**Path:** EU → USA → EU

| Connection             | Distance  | Typical RTT    | Notes                                                                         |
| ---------------------- | --------- | -------------- | ----------------------------------------------------------------------------- |
| EU → USA               | ~6,000 km | 80-150 ms      | Depends on EU location (London: ~80ms, Frankfurt: ~100ms, Eastern EU: ~150ms) |
| USA → EU               | ~6,000 km | 80-150 ms      | Return path                                                                   |
| **Total (one-way)**    |           | **80-150 ms**  | ESP32 → Client or Client → ESP32                                              |
| **Total (round-trip)** |           | **160-300 ms** | Full request/response cycle                                                   |

**Real-World Example:**

- Machine in London, UK
- VPS in New York, USA
- Client in London, UK
- **Expected:** ~160-200 ms round-trip

### Scenario 2: USA VPS, EU Machine, USA Client

**Path:** EU → USA → USA (local)

| Connection             | Typical RTT    | Notes                                    |
| ---------------------- | -------------- | ---------------------------------------- |
| EU → USA               | 80-150 ms      | Machine to cloud                         |
| USA → USA              | 10-50 ms       | Cloud to client (depends on US location) |
| **Total (one-way)**    | **90-200 ms**  |                                          |
| **Total (round-trip)** | **180-400 ms** |                                          |

### Scenario 3: USA VPS, USA Machine, USA Client

**Path:** USA → USA → USA (all local)

| Connection             | Typical RTT   | Notes            |
| ---------------------- | ------------- | ---------------- |
| USA → USA              | 10-50 ms      | Machine to cloud |
| USA → USA              | 10-50 ms      | Cloud to client  |
| **Total (one-way)**    | **20-100 ms** |                  |
| **Total (round-trip)** | **40-200 ms** |                  |

---

## Latency Breakdown

### Components

1. **Network Propagation Delay**

   - Speed of light: ~5ms per 1,000 km
   - EU ↔ USA: ~30ms (one-way)
   - Asia ↔ USA: ~70ms (one-way)

2. **Routing Hops**

   - Each router adds ~1-5ms
   - Typical: 10-20 hops = 10-100ms additional

3. **Submarine Cable Latency**

   - Multiple cables with different paths
   - Can add 20-50ms depending on routing

4. **Cloud Processing**

   - WebSocket message relay: <1ms (in-memory)
   - Database lookups: <5ms (SQLite, indexed)
   - **Total processing:** <10ms

5. **TCP/WebSocket Overhead**
   - Connection establishment: ~100ms (first time only)
   - Message framing: <1ms per message

**Total Processing Overhead:** <10ms (negligible compared to network latency)

---

## Real-World Measurements

### Test Results (Example)

**Setup:**

- VPS: DigitalOcean New York (USA)
- Machine: London, UK
- Client: London, UK

| Operation                       | Latency | Notes              |
| ------------------------------- | ------- | ------------------ |
| WebSocket connection            | 120 ms  | Initial handshake  |
| Status message (ESP32 → Client) | 85 ms   | One-way            |
| Command (Client → ESP32)        | 90 ms   | One-way            |
| Full round-trip                 | 175 ms  | Command + response |

---

## Impact on User Experience

### Acceptable Latency Thresholds

| Operation                  | Acceptable | Noticeable  | Poor     |
| -------------------------- | ---------- | ----------- | -------- |
| Status updates (read-only) | <200 ms    | 200-400 ms  | >400 ms  |
| Button clicks (commands)   | <300 ms    | 300-500 ms  | >500 ms  |
| Temperature changes        | <500 ms    | 500-1000 ms | >1000 ms |
| Real-time graphs           | <150 ms    | 150-300 ms  | >300 ms  |

### BrewOS Use Cases

✅ **Well Within Acceptable Range:**

- Viewing machine status (read-only)
- Monitoring temperatures and pressure
- Viewing statistics and history
- Push notifications (one-way, async)

⚠️ **Noticeable but Acceptable:**

- Button clicks (start/stop brew, mode changes)
- Temperature setpoint changes
- Settings updates

❌ **May Feel Sluggish:**

- Rapid interactions (multiple quick clicks)
- Real-time pressure graphs (may appear slightly delayed)

---

## Optimization Strategies

### 1. Regional VPS Deployment

**Best Solution:** Deploy cloud service closer to users

| VPS Location       | EU Machine + EU Client | USA Machine + USA Client |
| ------------------ | ---------------------- | ------------------------ |
| **USA (New York)** | 160-300 ms             | 40-100 ms                |
| **EU (Frankfurt)** | 20-50 ms               | 160-300 ms               |

**Recommendation:**

- For EU users: Deploy in Frankfurt, Amsterdam, or London
- For USA users: Deploy in New York, Virginia, or your coast
- For global users: Deploy in multiple regions with DNS routing

### 2. Edge Locations / CDN

Use a CDN or edge computing platform:

- **Cloudflare Workers** - Deploy at edge locations worldwide
- **AWS Lambda@Edge** - Regional deployments
- **Fly.io** - Multi-region deployment

**Benefit:** Automatic routing to nearest edge location

### 3. Connection Optimization

**WebSocket Keep-Alive:**

- Maintain persistent connections (already implemented)
- Reduces connection overhead from ~100ms to <1ms per message

**Message Batching:**

- Batch multiple status updates (if needed)
- Not currently implemented (each message sent immediately)

**Compression:**

- Enable WebSocket compression (permessage-deflate)
- Reduces bandwidth but adds ~5-10ms processing
- Only beneficial for large messages

### 4. Database Optimization

**Current Implementation:**

- SQLite in-memory (sql.js)
- All operations <5ms
- Already optimized

**No further optimization needed** for database access.

---

## Deployment Recommendations

### Single Region Deployment

**For EU Users:**

```
VPS: Frankfurt, Germany (Hetzner, DigitalOcean, AWS)
Expected Latency: 20-50 ms
Cost: ~$5-10/month
```

**For USA Users:**

```
VPS: New York or Virginia (DigitalOcean, AWS, Linode)
Expected Latency: 20-80 ms
Cost: ~$5-10/month
```

### Multi-Region Deployment

**For Global Users:**

```
Primary: Frankfurt, Germany (EU users)
Secondary: New York, USA (Americas users)
Tertiary: Singapore (Asia users)

DNS: Route based on user location
Expected Latency: 20-50 ms (regional) / 100-200 ms (cross-region)
Cost: ~$15-30/month
```

### Edge Computing

**Cloudflare Workers:**

```
Deploy to 200+ edge locations
Automatic routing to nearest location
Expected Latency: 20-100 ms (depends on location)
Cost: ~$5/month + usage
```

---

## Testing Latency

### From ESP32

Add latency logging to `device-relay.ts`:

```typescript
// Track message timestamps
const messageStart = Date.now();
// ... relay message ...
const latency = Date.now() - messageStart;
console.log(`[Latency] ESP32 → Client: ${latency}ms`);
```

### From Web Client

Add latency measurement in browser:

```javascript
// In WebSocket message handler
const startTime = performance.now();
ws.send(JSON.stringify({ type: 'command', cmd: 'set_temp', ... }));
// ... wait for response ...
const latency = performance.now() - startTime;
console.log(`Round-trip latency: ${latency}ms`);
```

### Network Tools

```bash
# Test latency to VPS
ping your-cloud-domain.com

# Test WebSocket latency
wscat -c wss://your-cloud-domain.com/ws/client?token=...&device=...
```

---

## Current Architecture Limitations

1. **Single VPS Location**

   - All traffic routes through one location
   - No automatic geographic routing

2. **No Message Queuing**

   - Messages are relayed immediately
   - No buffering or batching

3. **No Compression**
   - WebSocket messages sent uncompressed
   - Could reduce bandwidth for large payloads

**These are intentional design decisions** for simplicity. The architecture can be extended if needed.

---

## Summary

### For EU Users with USA VPS

| Location              | Expected Round-Trip | User Experience                          |
| --------------------- | ------------------- | ---------------------------------------- |
| **EU (e.g., London)** | 160-300 ms          | ✅ Good - Acceptable for most operations |

### Recommendations

1. **For EU users:** Consider moving VPS to Frankfurt or Amsterdam

   - **Improvement:** 160-300ms → 20-50ms (5-10x faster)
   - **Cost:** Similar pricing

2. **For USA users:** Keep VPS in USA

   - **Expected:** 40-100ms (already optimal)

3. **For global scale:** Use edge computing (Cloudflare Workers)
   - Automatic routing to nearest location
   - 20-100ms worldwide

---

## See Also

- [Cloud Service README](./README.md) - Overall architecture
- [Deployment Guide](./Deployment.md) - VPS deployment instructions
- [Database Storage](./Database_Storage.md) - Database performance
