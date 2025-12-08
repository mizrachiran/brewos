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

  // BBW and Pre-infusion settings (enabled by default in demo)
  private bbwEnabled = true;
  private bbwTargetWeight = 36;
  private bbwDoseWeight = 18;
  private bbwStopOffset = 2;
  private bbwAutoTare = true;
  private preinfusionEnabled = true;
  private preinfusionOnMs = 3000;
  private preinfusionPauseMs = 5000;

  // Machine type (for diagnostics)
  private machineType: "dual_boiler" | "single_boiler" | "heat_exchanger" =
    "dual_boiler";

  // Power meter simulation - configured by default in demo mode
  private powerMeterEnabled = true;
  private powerMeterSource: "none" | "hardware" | "mqtt" = "hardware";
  private powerMeterType: string | null = "PZEM-004T V3";
  private powerMeterConnected = true;

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

    // Send initial device info (sent once on connect)
    this.emit({
      type: "device_info",
      deviceId: "demo-device-001",
      deviceName: "My ECM Synchronika",
      machineBrand: "ECM",
      machineModel: "Synchronika",
      machineType: "dual_boiler",
      firmwareVersion: "1.0.0-demo",
      hasPressureSensor: true, // Demo has pressure sensor installed
      // Power settings
      mainsVoltage: 220,
      maxCurrent: 13,
      // Eco mode settings
      ecoBrewTemp: 80,
      ecoTimeoutMinutes: 30,
      // Pre-infusion settings (enabled by default in demo)
      preinfusionEnabled: this.preinfusionEnabled,
      preinfusionOnMs: this.preinfusionOnMs,
      preinfusionPauseMs: this.preinfusionPauseMs,
      // User preferences (synced across devices)
      preferences: {
        firstDayOfWeek: "sunday",
        use24HourTime: false,
        temperatureUnit: "celsius",
        electricityPrice: 0.15,
        currency: "USD",
        lastHeatingStrategy: 1,
        initialized: true,  // In demo, always initialized
      },
    });

    // Send initial BBW settings (enabled by default in demo)
    this.emitBBWSettings();

    // Send initial unified status (machine, temps, scale, connections, etc.)
    this.emitFullStatus();

    // Send initial power meter status
    this.emitPowerMeterStatus();

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
        this.emitFullStatus();
        break;
      case "scale_disconnect":
        this.scaleConnected = false;
        this.emitFullStatus();
        break;
      case "scale_connect":
        this.scaleConnected = true;
        this.emitFullStatus();
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
        this.emitFullStatus();
        break;
      case "set_bbw":
        // Brew-by-weight settings - store and emit
        if (data.enabled !== undefined) this.bbwEnabled = data.enabled as boolean;
        if (data.targetWeight !== undefined) this.bbwTargetWeight = data.targetWeight as number;
        if (data.doseWeight !== undefined) this.bbwDoseWeight = data.doseWeight as number;
        if (data.stopOffset !== undefined) this.bbwStopOffset = data.stopOffset as number;
        if (data.autoTare !== undefined) this.bbwAutoTare = data.autoTare as boolean;
        console.log("[Demo] BBW settings updated:", data);
        this.emitBBWSettings();
        break;
      case "set_preinfusion":
        // Pre-infusion settings - store and emit
        if (data.enabled !== undefined) this.preinfusionEnabled = data.enabled as boolean;
        if (data.onTimeMs !== undefined) this.preinfusionOnMs = data.onTimeMs as number;
        if (data.pauseTimeMs !== undefined) this.preinfusionPauseMs = data.pauseTimeMs as number;
        console.log("[Demo] Pre-infusion settings updated:", data);
        this.emitPreinfusionSettings();
        break;
      case "set_preferences":
        // User preferences - just log in demo mode (they're stored in localStorage)
        console.log("[Demo] User preferences updated:", data);
        break;
      case "set_machine_info":
        // Update machine type for diagnostics
        if (data.machineType) {
          this.machineType = data.machineType as
            | "dual_boiler"
            | "single_boiler"
            | "heat_exchanger";
        }
        // Update device info
        this.emit({
          type: "device_info",
          deviceId: "demo-device-001",
          deviceName: (data.name as string) || "My ECM Synchronika",
          machineBrand: (data.brand as string) || "ECM",
          machineModel: (data.model as string) || "Synchronika",
          machineType: (data.machineType as string) || "dual_boiler",
          firmwareVersion: "1.0.0-demo",
          hasPressureSensor: true,
          mainsVoltage: 220,
          maxCurrent: 13,
          ecoBrewTemp: 80,
          ecoTimeoutMinutes: 30,
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
          hasPressureSensor: true,
          mainsVoltage: 220,
          maxCurrent: 13,
          ecoBrewTemp: 80,
          ecoTimeoutMinutes: 30,
        });
        break;
      case "record_maintenance":
        // Record maintenance event
        this.handleMaintenanceRecord(data.type as string);
        break;
      case "run_diagnostics":
        this.simulateDiagnostics();
        break;
      case "configure_power_meter":
        this.handleConfigurePowerMeter(data);
        break;
      case "start_power_meter_discovery":
        this.simulatePowerMeterDiscovery();
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

  private handleConfigurePowerMeter(data: Record<string, unknown>): void {
    const source = data.source as "none" | "hardware" | "mqtt";
    this.powerMeterSource = source;
    
    if (source === "hardware") {
      const meterType = data.meterType as string;
      this.powerMeterType = meterType === "auto" ? null : meterType;
      this.powerMeterEnabled = true;
      this.powerMeterConnected = this.powerMeterType !== null;
      
      console.log("[Demo] Power meter configured:", {
        source,
        meterType: this.powerMeterType,
      });
    } else if (source === "mqtt") {
      this.powerMeterType = "MQTT";
      this.powerMeterEnabled = true;
      this.powerMeterConnected = true;
    } else {
      this.powerMeterEnabled = false;
      this.powerMeterConnected = false;
      this.powerMeterType = null;
    }
    
    // Emit updated power meter status
    this.emitPowerMeterStatus();
  }

  private simulatePowerMeterDiscovery(): void {
    // Simulate auto-detection of power meter
    console.log("[Demo] Starting power meter auto-discovery...");
    
    // Simulate scanning different baud rates with progress
    const steps = [
      { step: 1, progress: "Scanning UART1..." },
      { step: 2, progress: "Trying 9600 baud (PZEM-004T, JSY, Eastron)..." },
      { step: 3, progress: "Trying 4800 baud (JSY, Eastron)..." },
      { step: 4, progress: "Trying 2400 baud (Eastron)..." },
    ];

    // Emit each step with delays
    steps.forEach(({ step, progress }, index) => {
      setTimeout(() => {
        this.emit({
          type: "power_meter_status",
          source: "hardware",
          connected: false,
          meterType: null,
          lastUpdate: null,
          reading: null,
          error: null,
          discovering: true,
          discoveryProgress: progress,
          discoveryStep: step,
          discoveryTotal: 4,
        });
      }, index * 600);
    });

    // Final result after all steps
    setTimeout(() => {
      // Discovery complete - simulate finding a PZEM-004T
      this.powerMeterType = "PZEM-004T V3";
      this.powerMeterEnabled = true;
      this.powerMeterConnected = true;
      this.powerMeterSource = "hardware";
      
      this.emit({
        type: "power_meter_status",
        source: "hardware",
        connected: true,
        meterType: "PZEM-004T V3",
        lastUpdate: Date.now(),
        reading: this.generatePowerMeterReading(),
        error: null,
        discovering: false,
      });
    }, steps.length * 600 + 400);
  }

  private generatePowerMeterReading(): {
    voltage: number;
    current: number;
    power: number;
    energy: number;
    frequency: number;
    powerFactor: number;
  } {
    // Generate realistic power readings based on machine state
    const baseVoltage = 230 + (Math.random() - 0.5) * 4; // 228-232V
    const frequency = 50 + (Math.random() - 0.5) * 0.2; // 49.9-50.1Hz
    
    let current: number;
    let power: number;
    let powerFactor: number;
    
    if (this.isHeating) {
      // Heating - high power draw
      current = 6.0 + Math.random() * 0.5; // ~1400W heater
      power = Math.round(baseVoltage * current * 0.98);
      powerFactor = 0.98;
    } else if (this.isBrewing) {
      // Brewing - pump running
      current = 0.2 + Math.random() * 0.1;
      power = Math.round(baseVoltage * current * 0.85);
      powerFactor = 0.85;
    } else {
      // Idle - just control boards
      current = 0.02 + Math.random() * 0.01;
      power = Math.round(baseVoltage * current * 0.6);
      powerFactor = 0.6;
    }
    
    return {
      voltage: Number(baseVoltage.toFixed(1)),
      current: Number(current.toFixed(3)),
      power,
      energy: 89.3 + this.shotsToday * 0.05, // Cumulative kWh
      frequency: Number(frequency.toFixed(1)),
      powerFactor: Number(powerFactor.toFixed(2)),
    };
  }

  private emitPowerMeterStatus(): void {
    if (!this.powerMeterEnabled || this.powerMeterSource === "none") {
      this.emit({
        type: "power_meter_status",
        source: "none",
        connected: false,
        meterType: null,
        lastUpdate: null,
        reading: null,
        error: null,
      });
      return;
    }
    
    this.emit({
      type: "power_meter_status",
      source: this.powerMeterSource,
      connected: this.powerMeterConnected,
      meterType: this.powerMeterType,
      lastUpdate: this.powerMeterConnected ? Date.now() : null,
      reading: this.powerMeterConnected ? this.generatePowerMeterReading() : null,
      error: this.powerMeterConnected ? null : "No response from meter",
    });
  }

  private simulateDiagnostics(): void {
    // Get current machine type from the last emitted device info
    const machineType = this.machineType;

    // Define all possible test results with realistic values
    // Based on ECM_Control_Board_Specification_v2.20 hardware components
    const allTests: Record<
      number,
      {
        status: number;
        rawValue: number;
        min: number;
        max: number;
        message: string;
      }
    > = {
      // Temperature Sensors (T1, T2)
      0x01: {
        status: 0,
        rawValue: 2450,
        min: 2000,
        max: 3000,
        message: "25.2°C - NTC sensor OK",
      },
      0x02: {
        status: 0,
        rawValue: 2380,
        min: 2000,
        max: 3000,
        message: "26.1°C - NTC sensor OK",
      },

      // Pressure Sensor (P1)
      0x04: {
        status: 0,
        rawValue: 102,
        min: 50,
        max: 500,
        message: "0.0 bar - Transducer OK",
      },

      // Water Level Sensors (S1, S2, S3)
      0x05: {
        status: 2,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "Low level - Fill reservoir",
      }, // Warning
      0x0e: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "Probe circuit OK - Level normal",
      },

      // Brew Control (S4)
      0x0f: {
        status: 0,
        rawValue: 0,
        min: 0,
        max: 1,
        message: "Switch released - OK",
      },

      // Heater SSRs (SSR1, SSR2)
      0x06: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "SSR trigger activated",
      },
      0x07: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "SSR trigger activated",
      },

      // Relays (K1, K2, K3)
      0x10: {
        status: 3,
        rawValue: 0,
        min: 0,
        max: 0,
        message: "Not installed",
      }, // Optional - not all machines have indicator
      0x08: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "Relay click detected",
      },
      0x09: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "Relay click detected",
      },

      // Communication
      0x0b: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "UART link 921600 baud OK",
      },
      // Power meter test - will be set dynamically based on configuration
      0x0a: {
        status: 3,
        rawValue: 0,
        min: 0,
        max: 0,
        message: "Not configured",
      },

      // User Interface
      0x0c: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "Beep confirmed",
      },
      0x0d: {
        status: 0,
        rawValue: 1,
        min: 0,
        max: 1,
        message: "LED blink confirmed",
      },
    };

    // Determine which tests to run based on machine type
    const testsToRun: number[] = [];

    // Temperature sensors - machine type specific
    if (machineType === "dual_boiler") {
      testsToRun.push(0x01); // Brew NTC
      testsToRun.push(0x02); // Steam NTC
    } else if (machineType === "single_boiler") {
      testsToRun.push(0x01); // Brew NTC only
    } else if (machineType === "heat_exchanger") {
      testsToRun.push(0x02); // Steam NTC only (HX uses steam boiler temp)
    }

    // Pressure sensor (optional - let's say this user has it installed)
    testsToRun.push(0x04);
    allTests[0x04] = {
      status: 0,
      rawValue: 102,
      min: 50,
      max: 500,
      message: "0.0 bar - Transducer OK",
    };

    // Water level sensors (required for all)
    testsToRun.push(0x05); // Reservoir + tank

    // Steam boiler level probe - only for machines with steam boilers
    if (machineType === "dual_boiler" || machineType === "heat_exchanger") {
      testsToRun.push(0x0e); // Steam boiler level probe
    }

    // Brew switch (required for all)
    testsToRun.push(0x0f);

    // Heater SSRs - machine type specific
    if (machineType === "dual_boiler") {
      testsToRun.push(0x06); // Brew SSR
      testsToRun.push(0x07); // Steam SSR
    } else if (machineType === "single_boiler") {
      testsToRun.push(0x06); // Brew SSR only
    } else if (machineType === "heat_exchanger") {
      testsToRun.push(0x07); // Steam SSR only
    }

    // Relays (required)
    testsToRun.push(0x10); // Water LED relay (optional)
    testsToRun.push(0x08); // Pump relay
    testsToRun.push(0x09); // Solenoid relay

    // Communication
    testsToRun.push(0x0b); // ESP32
    testsToRun.push(0x0a); // Power meter (optional)

    // Update power meter test based on configuration
    if (this.powerMeterEnabled && this.powerMeterSource === "hardware") {
      if (this.powerMeterConnected && this.powerMeterType) {
        // Power meter configured and working
        const reading = this.generatePowerMeterReading();
        allTests[0x0a] = {
          status: 0, // Pass
          rawValue: Math.round(reading.voltage * 10), // Voltage * 10
          min: 2000, // 200V
          max: 2600, // 260V
          message: `${reading.voltage}V ${reading.current}A - ${this.powerMeterType} OK`,
        };
      } else {
        // Power meter configured but not responding
        allTests[0x0a] = {
          status: 1, // Fail
          rawValue: 0,
          min: 0,
          max: 0,
          message: "No response from meter",
        };
      }
    } else {
      // Power meter not configured - skip
      allTests[0x0a] = {
        status: 3, // Skip
        rawValue: 0,
        min: 0,
        max: 0,
        message: "Not configured",
      };
    }

    // User interface
    testsToRun.push(0x0c); // Buzzer
    testsToRun.push(0x0d); // LED

    // Sort tests by ID for consistent order
    testsToRun.sort((a, b) => a - b);

    // Build final test list
    const tests = testsToRun.map((testId) => ({
      testId,
      ...allTests[testId],
    }));

    // Emit header first (tests starting)
    this.emit({
      type: "diagnostics_header",
      testCount: tests.length,
      passCount: 0,
      failCount: 0,
      warnCount: 0,
      skipCount: 0,
      isComplete: false,
      durationMs: 0,
    });

    // Emit each test result with realistic delays
    const startTime = Date.now();
    let currentPass = 0;
    let currentFail = 0;
    let currentWarn = 0;
    let currentSkip = 0;

    tests.forEach((test, index) => {
      setTimeout(() => {
        // Update counts
        if (test.status === 0) currentPass++;
        else if (test.status === 1) currentFail++;
        else if (test.status === 2) currentWarn++;
        else if (test.status === 3) currentSkip++;

        // Emit result
        this.emit({
          type: "diagnostics_result",
          testId: test.testId,
          status: test.status,
          rawValue: test.rawValue,
          expectedMin: test.min,
          expectedMax: test.max,
          message: test.message,
        });

        // If last test, emit final header with complete stats
        if (index === tests.length - 1) {
          setTimeout(() => {
            this.emit({
              type: "diagnostics_header",
              testCount: tests.length,
              passCount: currentPass,
              failCount: currentFail,
              warnCount: currentWarn,
              skipCount: currentSkip,
              isComplete: true,
              durationMs: Date.now() - startTime,
            });
          }, 100);
        }
      }, (index + 1) * 200); // 200ms between each test
    });
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

    if (mode === "on") {
      // Machine is on - handle strategy change
      if (prevMode !== "on") {
        // Just turned ON - start tracking uptime
        this.machineOnTimestamp = Date.now();
        this.isHeating = true;
      }
      // Store and log heating strategy if provided (supports changing while on)
      if (strategy !== undefined) {
        const prevStrategy = this.heatingStrategy;
        this.heatingStrategy = strategy as 0 | 1 | 2 | 3;
        if (prevStrategy !== strategy) {
          console.log("[Demo] Heating strategy changed to:", strategy);
        }
      } else if (prevMode !== "on") {
        // Only set default strategy when first turning on
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
    this.emitFullStatus();
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
    // Emits unified status with all machine data
    this.simulationInterval = setInterval(() => {
      this.simulateStep();
    }, 500);

    // Stats and power meter update - runs every 5 seconds
    // Note: ESP32 info and connection status are included in unified status
    this.statsInterval = setInterval(() => {
      this.emitStats();
      // Update power meter reading if configured
      if (this.powerMeterEnabled && this.powerMeterConnected) {
        this.emitPowerMeterStatus();
      }
    }, 5000);
  }

  private simulateStep(): void {
    // Temperature simulation
    this.simulateTemperatures();

    // Brewing simulation
    if (this.isBrewing) {
      this.simulateBrewing();
    }

    // Emit unified status (includes machine, temps, scale, etc.)
    this.emitFullStatus();
  }

  private simulateTemperatures(): void {
    const heatingRate = 1.5; // °C per tick when heating
    const coolingRate = 0.3; // °C per tick when cooling
    const noise = () => (Math.random() - 0.5) * 0.2;

    if (this.machineMode === "on") {
      // Determine heating behavior based on strategy
      const strategy = this.heatingStrategy ?? 1; // Default to Sequential

      // Use consistent thresholds to avoid gaps
      const brewAtTarget = this.brewTemp >= this.targetBrewTemp - 0.5;
      const steamAtTarget = this.steamTemp >= this.targetSteamTemp - 1;
      const brewNeedsHeat = !brewAtTarget; // Heat until at target
      const steamNeedsHeat = !steamAtTarget; // Heat until at target

      // Determine which boilers should heat based on strategy
      let heatBrew = false;
      let heatSteam = false;

      switch (strategy) {
        case 0: // Brew Only
          heatBrew = brewNeedsHeat;
          heatSteam = false; // Never heat steam
          break;
        case 1: // Sequential - brew first, then steam
          heatBrew = brewNeedsHeat;
          heatSteam = brewAtTarget && steamNeedsHeat; // Only heat steam after brew is ready
          break;
        case 2: // Parallel - both at once
          heatBrew = brewNeedsHeat;
          heatSteam = steamNeedsHeat;
          break;
        case 3: {
          // Smart Stagger - staggered start, slight delay for steam
          heatBrew = brewNeedsHeat;
          // Start steam when brew is 50% there or brew is at target
          const brewProgress = this.brewTemp / this.targetBrewTemp;
          heatSteam = steamNeedsHeat && (brewProgress > 0.5 || brewAtTarget);
          break;
        }
      }

      // Update isHeating flag based on whether any boiler needs heat
      const anyHeating = heatBrew || heatSteam;
      const bothAtTarget =
        strategy === 0
          ? brewAtTarget // Brew Only: only brew matters
          : brewAtTarget && steamAtTarget; // Other strategies: both matter

      if (anyHeating && !this.isHeating) {
        this.isHeating = true;
      } else if (bothAtTarget && this.isHeating) {
        this.isHeating = false;
      }

      // Brew boiler temperature
      if (heatBrew) {
        this.brewTemp += heatingRate + noise();
      } else if (this.brewTemp > this.targetBrewTemp + 0.5) {
        this.brewTemp -= coolingRate;
      } else {
        // Stable at target with small fluctuation
        this.brewTemp = this.targetBrewTemp + noise();
      }

      // Steam boiler temperature
      if (heatSteam) {
        this.steamTemp += heatingRate * 1.2 + noise();
      } else if (strategy === 0) {
        // Brew Only: steam cools down or stays ambient
        if (this.steamTemp > 26) {
          this.steamTemp -= coolingRate * 0.5;
        }
      } else if (this.steamTemp > this.targetSteamTemp + 1) {
        this.steamTemp -= coolingRate;
      } else if (steamAtTarget) {
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

  /**
   * Unified status broadcast - matches ESP32 broadcastFullStatus() format
   * Single comprehensive message containing all machine state
   */
  private emitFullStatus(): void {
    const state = this.getMachineState();

    this.emit({
      type: "status",
      // Machine section
      machine: {
        state,
        mode: this.machineMode,
        isHeating: this.isHeating,
        isBrewing: this.isBrewing,
        heatingStrategy: this.heatingStrategy,
        machineOnTimestamp: this.machineOnTimestamp,
        lastShotTimestamp: this.lastShotTimestamp,
      },
      // Temperatures section
      temps: {
        brew: {
          current: Number(this.brewTemp.toFixed(1)),
          setpoint: this.targetBrewTemp,
        },
        steam: {
          current: Number(this.steamTemp.toFixed(1)),
          setpoint: this.targetSteamTemp,
        },
        group: Number(this.groupTemp.toFixed(1)),
      },
      // Pressure
      pressure: Number(this.pressure.toFixed(1)),
      // Power section
      power: {
        current: Math.round(
          this.isHeating ? 1400 + Math.random() * 200 : this.isBrewing ? 50 : 5
        ),
        voltage: 220,
      },
      // Water section
      water: {
        tankLevel: "ok",
      },
      // Scale section
      scale: {
        connected: this.scaleConnected,
        name: this.scaleConnected ? "Acaia Lunar" : "",
        scaleType: this.scaleConnected ? "acaia" : "",
        weight: Number(this.scaleWeight.toFixed(1)),
        flowRate: Number(this.flowRate.toFixed(1)),
        stable: !this.isBrewing || this.flowRate < 0.5,
        battery: 85,
      },
      // Shot section (active brew data)
      shot: {
        active: this.isBrewing,
        startTime: this.shotStartTime,
        duration: this.isBrewing ? Date.now() - this.shotStartTime : 0,
        weight: Number(this.shotWeight.toFixed(1)),
        flowRate: Number(this.flowRate.toFixed(1)),
      },
      // Connections section
      connections: {
        pico: true,
        wifi: true,
        mqtt: true,
        scale: this.scaleConnected,
      },
      // WiFi section (full status)
      wifi: {
        connected: true,
        ssid: "HomeNetwork",
        ip: "192.168.1.42",
        rssi: -58,
        apMode: false,
        staticIp: false,
        gateway: "192.168.1.1",
        subnet: "255.255.255.0",
        dns1: "192.168.1.1",
        dns2: "8.8.8.8",
      },
      // MQTT section
      mqtt: {
        enabled: true,
        connected: true,
        broker: "homeassistant.local",
        topic: "brewos",
      },
      // ESP32 info
      esp32: {
        version: "1.0.0-demo",
        freeHeap: 175000 + Math.floor(Math.random() * 10000),
        uptime: Date.now(),
      },
      // Stats section (real-time updates)
      stats: {
        shotsToday: this.shotsToday,
        sessionShots: this.shotsToday, // In demo, session = today
        daily: {
          shotCount: this.shotsToday,
          avgBrewTimeMs: 28500, // ~28.5 seconds average
          totalKwh: 0.8 + this.shotsToday * 0.05,
        },
        lifetime: {
          totalShots: this.totalShots,
          avgBrewTimeMs: 28500,
          totalKwh: 89.3,
        },
      },
    });
  }

  private emitBBWSettings(): void {
    this.emit({
      type: "bbw_settings",
      enabled: this.bbwEnabled,
      targetWeight: this.bbwTargetWeight,
      doseWeight: this.bbwDoseWeight,
      stopOffset: this.bbwStopOffset,
      autoTare: this.bbwAutoTare,
    });
  }

  private emitPreinfusionSettings(): void {
    this.emit({
      type: "preinfusion_settings",
      enabled: this.preinfusionEnabled,
      onTimeMs: this.preinfusionOnMs,
      pauseTimeMs: this.preinfusionPauseMs,
    });
  }

  private emitStats(): void {
    const sessionUptime = this.machineOnTimestamp
      ? Date.now() - this.machineOnTimestamp
      : 0;

    const now = Date.now();
    const day = 24 * 60 * 60 * 1000;

    this.emit({
      type: "stats",
      // Lifetime totals
      totalShots: this.totalShots,
      totalSteamCycles: 234,
      totalKwh: 89.3,
      totalOnTimeMinutes: 15420,

      // Today's stats
      shotsToday: this.shotsToday,
      kwhToday: 0.8 + this.shotsToday * 0.05,
      onTimeToday: Math.floor(sessionUptime / 60000),

      // Maintenance tracking (legacy)
      shotsSinceDescale: 145,
      shotsSinceGroupClean: 12,
      shotsSinceBackflush: 45,
      lastDescaleTimestamp: now - 30 * day,
      lastGroupCleanTimestamp: now - 2 * day,
      lastBackflushTimestamp: now - 7 * day,

      // Maintenance (structured)
      maintenance: {
        shotsSinceBackflush: 45,
        shotsSinceGroupClean: 12,
        shotsSinceDescale: 145,
        lastBackflushTimestamp: now - 7 * day,
        lastGroupCleanTimestamp: now - 2 * day,
        lastDescaleTimestamp: now - 30 * day,
      },

      // Brew time stats
      avgBrewTimeMs: 28500,
      minBrewTimeMs: 22000,
      maxBrewTimeMs: 35000,

      // Period counts
      dailyCount: this.shotsToday,
      weeklyCount: 28,
      monthlyCount: 124,

      // Session
      sessionShots: this.shotsToday,
      sessionStartTimestamp: this.machineOnTimestamp || now,

      // Period stats (for power tab)
      daily: {
        shotCount: this.shotsToday,
        totalBrewTimeMs: this.shotsToday * 28500,
        avgBrewTimeMs: 28500,
        minBrewTimeMs: 26000,
        maxBrewTimeMs: 31000,
        totalKwh: 0.8 + this.shotsToday * 0.05,
      },
      weekly: {
        shotCount: 28,
        totalBrewTimeMs: 798000,
        avgBrewTimeMs: 28500,
        minBrewTimeMs: 22000,
        maxBrewTimeMs: 35000,
        totalKwh: 7.2,
      },
      monthly: {
        shotCount: 124,
        totalBrewTimeMs: 3534000,
        avgBrewTimeMs: 28500,
        minBrewTimeMs: 21000,
        maxBrewTimeMs: 38000,
        totalKwh: 28.5,
      },
      lifetime: {
        totalShots: this.totalShots,
        totalSteamCycles: 234,
        totalKwh: 89.3,
        totalOnTimeMinutes: 15420,
        totalBrewTimeMs: this.totalShots * 28500,
        avgBrewTimeMs: 28500,
        minBrewTimeMs: 18000,
        maxBrewTimeMs: 42000,
        firstShotTimestamp: Math.floor((now - 180 * day) / 1000), // Started 6 months ago
      },
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
