import type { WebSocketMessage, ConnectionState, IConnection } from "./types";

type MessageHandler = (message: WebSocketMessage) => void;
type StateHandler = (state: ConnectionState) => void;

/**
 * Simulated connection for demo mode
 * Generates realistic espresso machine data without needing real hardware
 */
export class DemoConnection implements IConnection {
  private state: ConnectionState = "disconnected";
  private messageHandlers = new Set<MessageHandler>();
  private stateHandlers = new Set<StateHandler>();
  private simulationInterval: ReturnType<typeof setInterval> | null = null;
  private statsInterval: ReturnType<typeof setInterval> | null = null;
  private isDisconnected = false; // Flag to abort pending connect()

  // Simulation state
  private machineMode: "standby" | "on" | "eco" = "standby";
  private brewTemp = 25;
  private steamTemp = 25;
  private groupTemp = 22;
  private targetBrewTemp = 93.5;
  private targetSteamTemp = 145;
  private isHeating = false;
  private isBrewing = false;
  private shotStartTime = 0;
  private shotWeight = 0;
  private pressure = 0;
  private shotsToday = 3;
  private totalShots = 1247;
  private machineOnTimestamp: number | null = null; // When machine was turned ON
  private heatingStrategy: 0 | 1 | 2 | 3 | null = null; // Active heating strategy
  private lastShotTimestamp: number | null = Date.now() - 2 * 60 * 60 * 1000; // 2 hours ago for demo

  // Scale simulation
  private scaleConnected = true;
  private scaleWeight = 0;
  private flowRate = 0;

  async connect(): Promise<void> {
    this.isDisconnected = false;
    this.setState("connecting");

    // Simulate connection delay
    await new Promise((r) => setTimeout(r, 800));

    // Check if disconnected while waiting (handles React StrictMode double-mount)
    if (this.isDisconnected) {
      return;
    }

    this.setState("connected");
    this.startSimulation();

    // Send initial device info
    this.emit({
      type: "device_info",
      deviceId: "demo-device-001",
      deviceName: "My ECM Synchronika",
      machineBrand: "ECM",
      machineModel: "Synchronika",
      machineType: "dual_boiler",
      firmwareVersion: "1.0.0-demo",
    });

    // Send initial ESP32 status
    this.emitEspStatus();

    // Send initial machine/pico status immediately
    this.emitPicoStatus();

    // Send initial scale status
    this.emitScaleStatus();

    // Send initial stats
    this.emitStats();
  }

  disconnect(): void {
    this.isDisconnected = true; // Abort any pending connect()
    this.stopSimulation();
    this.setState("disconnected");
  }

  send(type: string, payload: Record<string, unknown> = {}): void {
    if (type === "command") {
      this.sendCommand(payload.cmd as string, payload);
    }
  }

  sendCommand(cmd: string, data: Record<string, unknown> = {}): void {
    console.log("[Demo] Command received:", cmd, data);

    switch (cmd) {
      case "set_mode":
        this.handleSetMode(data.mode as string, data.strategy as number);
        break;
      case "brew_start":
        this.startBrewing();
        break;
      case "brew_stop":
        this.stopBrewing();
        break;
      case "set_temp":
        // Handle temperature setpoint changes
        if (data.boiler === "brew") {
          this.targetBrewTemp = data.temp as number;
        } else if (data.boiler === "steam") {
          this.targetSteamTemp = data.temp as number;
        }
        break;
      case "set_brew_temp":
        this.targetBrewTemp = data.temp as number;
        break;
      case "set_steam_temp":
        this.targetSteamTemp = data.temp as number;
        break;
      case "tare":
      case "scale_tare":
        this.scaleWeight = 0;
        this.shotWeight = 0;
        this.emitScaleStatus();
        break;
      case "scale_disconnect":
        this.scaleConnected = false;
        this.emitScaleStatus();
        break;
      case "scale_connect":
        this.scaleConnected = true;
        this.emitScaleStatus();
        break;
      case "scale_scan":
        // Simulate BLE scan finding scales
        this.simulateScaleScan();
        break;
      case "scale_scan_stop":
        // Just acknowledge
        break;
      case "scale_reset":
        this.scaleWeight = 0;
        this.shotWeight = 0;
        this.flowRate = 0;
        this.emitScaleStatus();
        break;
      case "set_bbw":
        // Brew-by-weight settings - just acknowledge
        console.log("[Demo] BBW settings received:", data);
        break;
      case "set_machine_info":
        // Update device info
        this.emit({
          type: "device_info",
          deviceId: "demo-device-001",
          deviceName: (data.name as string) || "My ECM Synchronika",
          machineBrand: (data.brand as string) || "ECM",
          machineModel: (data.model as string) || "Synchronika",
          machineType: (data.machineType as string) || "dual_boiler",
          firmwareVersion: "1.0.0-demo",
        });
        break;
      case "set_device_info":
        // First run wizard device info
        this.emit({
          type: "device_info",
          deviceId: "demo-device-001",
          deviceName: (data.deviceName as string) || "My ECM Synchronika",
          machineBrand: (data.machineBrand as string) || "ECM",
          machineModel: (data.machineModel as string) || "Synchronika",
          machineType: (data.machineType as string) || "dual_boiler",
          firmwareVersion: "1.0.0-demo",
        });
        break;
      case "record_maintenance":
        // Record maintenance event
        this.handleMaintenanceRecord(data.type as string);
        break;
      case "set_power":
      case "set_eco":
      case "mqtt_config":
      case "mqtt_test":
      case "set_cloud_config":
        // Acknowledge settings commands (fire and forget)
        console.log("[Demo] Settings command acknowledged:", cmd);
        break;
      default:
        console.log("[Demo] Unhandled command:", cmd, data);
    }
  }

  private simulateScaleScan(): void {
    // Emit scan results after short delays to simulate BLE discovery
    setTimeout(() => {
      this.emit({
        type: "scan_result",
        address: "AA:BB:CC:DD:EE:01",
        name: "Acaia Lunar",
        rssi: -55,
      });
    }, 500);

    setTimeout(() => {
      this.emit({
        type: "scan_result",
        address: "AA:BB:CC:DD:EE:02",
        name: "Felicita Arc",
        rssi: -62,
      });
    }, 1200);

    setTimeout(() => {
      this.emit({
        type: "scan_result",
        address: "AA:BB:CC:DD:EE:03",
        name: "Decent Scale",
        rssi: -70,
      });
    }, 1800);

    setTimeout(() => {
      this.emit({ type: "scan_complete" });
    }, 3000);
  }

  private handleMaintenanceRecord(maintenanceType: string): void {
    // Update maintenance timestamps
    const now = Date.now();

    switch (maintenanceType) {
      case "descale":
        this.emit({
          type: "stats",
          shotsSinceDescale: 0,
          lastDescaleTimestamp: now,
        });
        break;
      case "group_clean":
        this.emit({
          type: "stats",
          shotsSinceGroupClean: 0,
          lastGroupCleanTimestamp: now,
        });
        break;
      case "backflush":
        this.emit({
          type: "stats",
          shotsSinceBackflush: 0,
          lastBackflushTimestamp: now,
        });
        break;
    }
  }

  private handleSetMode(mode: string, strategy?: number): void {
    const prevMode = this.machineMode;
    this.machineMode = mode as "standby" | "on" | "eco";

    if (mode === "on" && prevMode !== "on") {
      // Machine turned ON - start tracking uptime
      this.machineOnTimestamp = Date.now();
      this.isHeating = true;
      // Store and log heating strategy if provided
      if (strategy !== undefined) {
        this.heatingStrategy = strategy as 0 | 1 | 2 | 3;
        console.log("[Demo] Heating with strategy:", strategy);
      } else {
        this.heatingStrategy = 1; // Default to Sequential
      }
    } else if (mode === "standby") {
      // Machine turned OFF - reset uptime and strategy
      this.machineOnTimestamp = null;
      this.heatingStrategy = null;
      this.isHeating = false;
      // Gradual cooldown will happen in simulation
    } else if (mode === "eco" && prevMode !== "eco") {
      // Eco mode also counts as "on" for uptime purposes
      if (!this.machineOnTimestamp) {
        this.machineOnTimestamp = Date.now();
      }
      this.isHeating = this.brewTemp < this.targetBrewTemp - 10;
      // Keep existing strategy in eco mode
    }

    // Emit status immediately on mode change to prevent UI flicker
    this.emitPicoStatus();
    this.emitEspStatus();
  }

  private startBrewing(): void {
    if (this.machineMode !== "on") return;

    this.isBrewing = true;
    this.shotStartTime = Date.now();
    this.shotWeight = 0;
    this.pressure = 0;
    this.flowRate = 0;

    this.emit({ type: "event", event: "shot_start" });
  }

  private stopBrewing(): void {
    if (!this.isBrewing) return;

    this.isBrewing = false;
    this.shotsToday++;
    this.totalShots++;
    this.lastShotTimestamp = Date.now();

    this.emit({
      type: "event",
      event: "shot_end",
      duration: Date.now() - this.shotStartTime,
      weight: this.shotWeight,
    });

    // Reset brewing state
    setTimeout(() => {
      this.pressure = 0;
      this.flowRate = 0;
    }, 500);
  }

  private startSimulation(): void {
    // Main simulation loop - runs every 500ms for smooth updates
    this.simulationInterval = setInterval(() => {
      this.simulateStep();
    }, 500);

    // Stats and ESP status update - runs every 5 seconds
    // Note: Uptime is now calculated in UI from machineOnTimestamp, so no need for frequent updates
    this.statsInterval = setInterval(() => {
      this.emitStats();
      this.emitEspStatus();
    }, 5000);
  }

  private simulateStep(): void {
    // Temperature simulation
    this.simulateTemperatures();

    // Brewing simulation
    if (this.isBrewing) {
      this.simulateBrewing();
    }

    // Emit status update
    this.emitPicoStatus();

    // Emit scale status if brewing
    if (this.isBrewing || this.scaleWeight > 0) {
      this.emitScaleStatus();
    }
  }

  private simulateTemperatures(): void {
    const heatingRate = 1.5; // °C per tick when heating
    const coolingRate = 0.3; // °C per tick when cooling
    const noise = () => (Math.random() - 0.5) * 0.2;

    if (this.machineMode === "on") {
      // Determine if heating is needed using hysteresis
      const brewNeedsHeat = this.brewTemp < this.targetBrewTemp - 2;
      const steamNeedsHeat = this.steamTemp < this.targetSteamTemp - 3;
      const brewAtTarget = this.brewTemp >= this.targetBrewTemp - 0.5;
      const steamAtTarget = this.steamTemp >= this.targetSteamTemp - 1;

      // Use hysteresis for isHeating flag
      if ((brewNeedsHeat || steamNeedsHeat) && !this.isHeating) {
        this.isHeating = true;
      } else if (brewAtTarget && steamAtTarget && this.isHeating) {
        this.isHeating = false;
      }

      // Brew boiler temperature
      if (this.brewTemp < this.targetBrewTemp - 0.5) {
        this.brewTemp += heatingRate + noise();
      } else if (this.brewTemp > this.targetBrewTemp + 0.5) {
        this.brewTemp -= coolingRate;
      } else {
        // Stable at target with small fluctuation
        this.brewTemp = this.targetBrewTemp + noise();
      }

      // Steam boiler temperature
      if (this.steamTemp < this.targetSteamTemp - 1) {
        this.steamTemp += heatingRate * 1.2 + noise();
      } else if (this.steamTemp > this.targetSteamTemp + 1) {
        this.steamTemp -= coolingRate;
      } else {
        // Stable at target with small fluctuation
        this.steamTemp = this.targetSteamTemp + noise();
      }

      // Group head (follows brew boiler with lag)
      const targetGroup = this.brewTemp * 0.88;
      this.groupTemp += (targetGroup - this.groupTemp) * 0.1;
    } else if (this.machineMode === "eco") {
      // Eco mode: maintain lower temps with hysteresis to prevent oscillation
      const ecoBrewTarget = this.targetBrewTemp - 15;

      // Use hysteresis: start heating below target-3, stop above target-1
      if (this.brewTemp < ecoBrewTarget - 3 && !this.isHeating) {
        this.isHeating = true;
      } else if (this.brewTemp > ecoBrewTarget - 1 && this.isHeating) {
        this.isHeating = false;
      }

      // Apply temperature changes
      if (this.isHeating) {
        this.brewTemp += heatingRate * 0.4;
      } else if (this.brewTemp > ecoBrewTarget + 1) {
        this.brewTemp -= coolingRate * 0.3;
      } else {
        // Small random fluctuation around target
        this.brewTemp = ecoBrewTarget + noise() * 2;
      }

      // Steam boiler cools down in eco mode
      this.steamTemp = Math.max(25, this.steamTemp - coolingRate * 0.3);
      this.groupTemp = Math.max(22, this.groupTemp - coolingRate * 0.2);
    } else {
      // Standby: cool down
      if (this.brewTemp > 26) {
        this.brewTemp -= coolingRate;
      }
      if (this.steamTemp > 26) {
        this.steamTemp -= coolingRate * 0.8;
      }
      if (this.groupTemp > 23) {
        this.groupTemp -= coolingRate * 0.5;
      }
      this.isHeating = false;
    }

    // Clamp values
    this.brewTemp = Math.max(20, Math.min(105, this.brewTemp));
    this.steamTemp = Math.max(20, Math.min(160, this.steamTemp));
    this.groupTemp = Math.max(20, Math.min(95, this.groupTemp));
  }

  private simulateBrewing(): void {
    const elapsed = (Date.now() - this.shotStartTime) / 1000;

    // Pressure ramp-up (0-8s: pre-infusion, 8-30s: extraction)
    if (elapsed < 3) {
      // Pre-infusion pressure ramp
      this.pressure = elapsed * 1.5;
    } else if (elapsed < 8) {
      // Pressure building
      this.pressure = 4.5 + (elapsed - 3) * 0.8;
    } else {
      // Full extraction pressure with slight variation
      this.pressure = 8.5 + (Math.random() - 0.5) * 0.5;
    }

    // Flow rate simulation (ml/s)
    if (elapsed < 5) {
      this.flowRate = elapsed * 0.4;
    } else {
      this.flowRate = 2 + (Math.random() - 0.5) * 0.3;
    }

    // Weight accumulation
    this.shotWeight += this.flowRate * 0.5;
    this.scaleWeight = this.shotWeight;

    // Auto-stop at ~36g (typical double shot)
    if (this.shotWeight >= 36) {
      this.stopBrewing();
    }
  }

  private emitPicoStatus(): void {
    const state = this.getMachineState();

    // Emit pico_status with temperature and sensor data
    this.emit({
      type: "pico_status",
      version: "1.0.0-demo",
      machineOnTimestamp: this.machineOnTimestamp,
      heatingStrategy: this.heatingStrategy,
      lastShotTimestamp: this.lastShotTimestamp,
      state,
      isHeating: this.isHeating,
      isBrewing: this.isBrewing,
      brewTemp: Number(this.brewTemp.toFixed(1)),
      steamTemp: Number(this.steamTemp.toFixed(1)),
      brewSetpoint: this.targetBrewTemp,
      steamSetpoint: this.targetSteamTemp,
      groupTemp: Number(this.groupTemp.toFixed(1)),
      pressure: Number(this.pressure.toFixed(1)),
      power: Math.round(
        this.isHeating ? 1400 + Math.random() * 200 : this.isBrewing ? 50 : 5
      ),
      voltage: 220,
      waterLevel: "ok",
      dripTrayFull: false,
      // Include mode in pico_status to avoid separate status message
      mode: this.machineMode,
    });
  }

  private emitScaleStatus(): void {
    this.emit({
      type: "scale_status",
      connected: this.scaleConnected,
      name: this.scaleConnected ? "Acaia Lunar" : "",
      scaleType: this.scaleConnected ? "acaia" : "",
      weight: Number(this.scaleWeight.toFixed(1)),
      flowRate: Number(this.flowRate.toFixed(1)),
      stable: !this.isBrewing || this.flowRate < 0.5,
      battery: 85,
    });
  }

  private emitEspStatus(): void {
    this.emit({
      type: "esp_status",
      version: "1.0.0-demo",
      freeHeap: 175000 + Math.floor(Math.random() * 10000), // Slight variation
      wifi: {
        connected: true,
        ssid: "Demo Network",
        ip: "192.168.1.100",
        rssi: -45 + Math.floor(Math.random() * 5), // Slight RSSI variation
        apMode: false,
      },
      mqtt: {
        enabled: true,
        connected: true,
        broker: "demo.brewos.local",
        topic: "brewos",
      },
    });
  }

  private emitStats(): void {
    const sessionUptime = this.machineOnTimestamp
      ? Date.now() - this.machineOnTimestamp
      : 0;

    this.emit({
      type: "stats",
      totalShots: this.totalShots,
      totalSteamCycles: 234,
      totalKwh: 89.3,
      totalOnTimeMinutes: 15420,
      shotsToday: this.shotsToday,
      kwhToday: 0.8 + this.shotsToday * 0.05,
      onTimeToday: Math.floor(sessionUptime / 60000), // Convert ms to minutes
      shotsSinceDescale: 145,
      shotsSinceGroupClean: 12,
      shotsSinceBackflush: 45,
      lastDescaleTimestamp: Date.now() - 30 * 24 * 60 * 60 * 1000,
      lastGroupCleanTimestamp: Date.now() - 2 * 24 * 60 * 60 * 1000,
      lastBackflushTimestamp: Date.now() - 7 * 24 * 60 * 60 * 1000,
      avgBrewTimeMs: 28500,
      minBrewTimeMs: 22000,
      maxBrewTimeMs: 35000,
      dailyCount: this.shotsToday,
      weeklyCount: 28,
      monthlyCount: 124,
      sessionShots: this.shotsToday,
      sessionStartTimestamp: this.machineOnTimestamp || Date.now(),
    });
  }

  private getMachineState(): string {
    if (this.machineMode === "standby") return "idle";
    if (this.isBrewing) return "brewing";

    // Eco mode has its own "ready" state at lower temps
    if (this.machineMode === "eco") {
      const ecoBrewTarget = this.targetBrewTemp - 15;
      if (this.isHeating) return "heating";
      // Consider "ready" in eco mode when brew temp is near eco target
      if (this.brewTemp >= ecoBrewTarget - 2) {
        return "ready";
      }
      return "heating";
    }

    // Normal "on" mode
    if (this.isHeating) return "heating";
    if (
      this.brewTemp >= this.targetBrewTemp - 2 &&
      this.steamTemp >= this.targetSteamTemp - 3
    ) {
      return "ready";
    }
    return "heating";
  }

  // Event handlers (same interface as Connection)
  onMessage(handler: MessageHandler): () => void {
    this.messageHandlers.add(handler);
    return () => this.messageHandlers.delete(handler);
  }

  onStateChange(handler: StateHandler): () => void {
    this.stateHandlers.add(handler);
    handler(this.state);
    return () => this.stateHandlers.delete(handler);
  }

  getState(): ConnectionState {
    return this.state;
  }

  private emit(message: WebSocketMessage): void {
    this.messageHandlers.forEach((h) => h(message));
  }

  private setState(state: ConnectionState): void {
    if (this.state !== state) {
      this.state = state;
      this.stateHandlers.forEach((h) => h(state));
    }
  }

  private stopSimulation(): void {
    if (this.simulationInterval) {
      clearInterval(this.simulationInterval);
      this.simulationInterval = null;
    }
    if (this.statsInterval) {
      clearInterval(this.statsInterval);
      this.statsInterval = null;
    }
  }
}

// Singleton for demo connection
let demoConnection: DemoConnection | null = null;

export function getDemoConnection(): DemoConnection {
  // Always disconnect any existing connection first to prevent duplicates (handles React StrictMode)
  if (demoConnection) {
    demoConnection.disconnect();
    demoConnection = null;
  }
  demoConnection = new DemoConnection();
  return demoConnection;
}

export function getActiveDemoConnection(): DemoConnection | null {
  return demoConnection;
}

export function clearDemoConnection(): void {
  if (demoConnection) {
    demoConnection.disconnect();
    demoConnection = null;
  }
}
