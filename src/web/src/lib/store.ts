import { create } from "zustand";
import { subscribeWithSelector } from "zustand/middleware";
import type {
  ConnectionState,
  MachineStatus,
  Temperatures,
  PowerStatus,
  PowerMeterStatus,
  CleaningStatus,
  WaterStatus,
  ScaleStatus,
  BBWSettings,
  PreinfusionSettings,
  ShotStatus,
  WiFiStatus,
  MQTTStatus,
  ESP32Info,
  PicoInfo,
  DeviceInfo,
  Statistics,
  LifetimeStats,
  PeriodStats,
  MaintenanceStats,
  Alert,
  LogEntry,
  ScaleScanResult,
  WebSocketMessage,
  UserPreferences,
  IConnection,
  DiagnosticReport,
  DiagnosticResult,
  DiagnosticHeader,
} from "./types";
import {
  diagStatusFromCode,
  getDiagnosticTestName,
  type DiagnosticTestId,
} from "./types";

interface BrewOSState {
  // Connection
  connectionState: ConnectionState;

  // Device identity
  device: DeviceInfo;

  // Machine
  machine: MachineStatus;
  temps: Temperatures;
  pressure: number;
  power: PowerStatus;
  cleaning: CleaningStatus;
  water: WaterStatus;

  // Scale
  scale: ScaleStatus;
  scaleScanning: boolean;
  scanResults: ScaleScanResult[];

  // Brew-by-weight
  bbw: BBWSettings;
  shot: ShotStatus;

  // Pre-infusion
  preinfusion: PreinfusionSettings;

  // Eco mode settings
  ecoMode: EcoModeSettings;

  // Network
  wifi: WiFiStatus;
  mqtt: MQTTStatus;

  // Device info
  esp32: ESP32Info;
  pico: PicoInfo;

  // Stats & logs
  stats: Statistics;
  alerts: Alert[];
  logs: LogEntry[];

  // Diagnostics
  diagnostics: DiagnosticReport;

  // User preferences (localStorage)
  preferences: UserPreferences;

  // Actions
  setConnectionState: (state: ConnectionState) => void;
  processMessage: (message: WebSocketMessage) => void;
  dismissAlert: (id: number) => void;
  clearLogs: () => void;
  addScanResult: (result: ScaleScanResult) => void;
  setScaleScanning: (scanning: boolean) => void;
  clearScanResults: () => void;
  setPreference: <K extends keyof UserPreferences>(
    key: K,
    value: UserPreferences[K]
  ) => void;

  // Diagnostics actions
  setDiagnosticsRunning: (running: boolean) => void;
  resetDiagnostics: () => void;
}

// Default state
const defaultMachine: MachineStatus = {
  state: "unknown",
  mode: "standby",
  isHeating: false,
  isBrewing: false,
  machineOnTimestamp: null,
  heatingStrategy: null,
  lastShotTimestamp: null,
};

const defaultTemps: Temperatures = {
  brew: { current: 0, setpoint: 93.5, max: 105 },
  steam: { current: 0, setpoint: 145, max: 160 },
  group: 0,
};

const defaultPower: PowerStatus = {
  current: 0,
  voltage: 220,
  maxCurrent: 13, // Default to 13A (UK standard)
  todayKwh: 0,
  totalKwh: 0,
};

interface EcoModeSettings {
  ecoBrewTemp: number;
  autoOffTimeout: number;
}

const defaultEcoMode: EcoModeSettings = {
  ecoBrewTemp: 80, // °C
  autoOffTimeout: 30, // minutes
};

const defaultCleaning: CleaningStatus = {
  brewCount: 0,
  reminderDue: false,
};

const defaultWater: WaterStatus = {
  tankLevel: "ok",
};

const defaultScale: ScaleStatus = {
  connected: false,
  name: "",
  type: "",
  weight: 0,
  flowRate: 0,
  stable: false,
  battery: 0,
};

const defaultBBW: BBWSettings = {
  enabled: false,
  targetWeight: 36,
  doseWeight: 18,
  stopOffset: 2,
  autoTare: true,
};

const defaultPreinfusion: PreinfusionSettings = {
  enabled: false,
  onTimeMs: 3000,    // 3 seconds pump on
  pauseTimeMs: 5000, // 5 seconds soak
};

const defaultShot: ShotStatus = {
  active: false,
  startTime: 0,
  duration: 0,
  weight: 0,
  flowRate: 0,
};

const defaultWifi: WiFiStatus = {
  connected: false,
  ssid: "",
  ip: "",
  rssi: 0,
  apMode: false,
  staticIp: false,
  gateway: "",
  subnet: "255.255.255.0",
  dns1: "",
  dns2: "",
};

const defaultMqtt: MQTTStatus = {
  enabled: false,
  connected: false,
  broker: "",
  topic: "brewos",
};

const defaultEsp32: ESP32Info = {
  version: "",
  freeHeap: 0,
};

const defaultPico: PicoInfo = {
  connected: false,
  version: "",
};

const defaultStats: Statistics = {
  // New enhanced structure
  lifetime: {
    totalShots: 0,
    totalSteamCycles: 0,
    totalKwh: 0,
    totalOnTimeMinutes: 0,
    totalBrewTimeMs: 0,
    avgBrewTimeMs: 0,
    minBrewTimeMs: 0,
    maxBrewTimeMs: 0,
    firstShotTimestamp: 0,
  },
  daily: {
    shotCount: 0,
    totalBrewTimeMs: 0,
    avgBrewTimeMs: 0,
    minBrewTimeMs: 0,
    maxBrewTimeMs: 0,
    totalKwh: 0,
  },
  weekly: {
    shotCount: 0,
    totalBrewTimeMs: 0,
    avgBrewTimeMs: 0,
    minBrewTimeMs: 0,
    maxBrewTimeMs: 0,
    totalKwh: 0,
  },
  monthly: {
    shotCount: 0,
    totalBrewTimeMs: 0,
    avgBrewTimeMs: 0,
    minBrewTimeMs: 0,
    maxBrewTimeMs: 0,
    totalKwh: 0,
  },
  maintenance: {
    shotsSinceBackflush: 0,
    shotsSinceGroupClean: 0,
    shotsSinceDescale: 0,
    lastBackflushTimestamp: 0,
    lastGroupCleanTimestamp: 0,
    lastDescaleTimestamp: 0,
  },
  sessionShots: 0,
  sessionStartTimestamp: 0,

  // Legacy compatibility (derived from new structure)
  totalShots: 0,
  totalSteamCycles: 0,
  totalKwh: 0,
  totalOnTimeMinutes: 0,
  shotsToday: 0,
  kwhToday: 0,
  onTimeToday: 0,
  shotsSinceDescale: 0,
  shotsSinceGroupClean: 0,
  shotsSinceBackflush: 0,
  lastDescaleTimestamp: 0,
  lastGroupCleanTimestamp: 0,
  lastBackflushTimestamp: 0,
  avgBrewTimeMs: 0,
  minBrewTimeMs: 0,
  maxBrewTimeMs: 0,
  dailyCount: 0,
  weeklyCount: 0,
  monthlyCount: 0,
};

const defaultDevice: DeviceInfo = {
  deviceId: "",
  deviceName: "My BrewOS",
  machineBrand: "",
  machineModel: "",
  machineType: "" as DeviceInfo["machineType"],
  firmwareVersion: "",
  hasPressureSensor: true, // Default true, will be updated from device
};

const defaultDiagnostics: DiagnosticReport = {
  header: {
    testCount: 0,
    passCount: 0,
    failCount: 0,
    warnCount: 0,
    skipCount: 0,
    isComplete: false,
    durationMs: 0,
  },
  results: [],
  isRunning: false,
  timestamp: 0,
};

// Default preferences (used as fallback)
const defaultPreferences: UserPreferences = {
  firstDayOfWeek: "sunday",
  use24HourTime: false,
  temperatureUnit: "celsius",
  electricityPrice: 0.15,
  currency: "USD",
};

// Load preferences from localStorage (fallback for offline/demo mode)
const loadPreferencesFromStorage = (): UserPreferences => {
  try {
    const saved = localStorage.getItem("brewos_preferences");
    if (saved) {
      return { ...defaultPreferences, ...JSON.parse(saved) };
    }
  } catch (e) {
    console.warn("Failed to load preferences from storage:", e);
  }
  return defaultPreferences;
};

// Save preferences to localStorage (cache for offline use)
const savePreferencesToStorage = (prefs: UserPreferences) => {
  try {
    localStorage.setItem("brewos_preferences", JSON.stringify(prefs));
  } catch (e) {
    console.warn("Failed to save preferences to storage:", e);
  }
};

// Detect browser locale for initial preferences setup
const detectBrowserPreferences = (): Partial<UserPreferences> => {
  const detected: Partial<UserPreferences> = {};
  
  // Detect 24-hour time preference from locale
  try {
    const date = new Date();
    const formatted = date.toLocaleTimeString(navigator.language, { hour: 'numeric' });
    detected.use24HourTime = !formatted.includes('AM') && !formatted.includes('PM');
  } catch {
    detected.use24HourTime = false;
  }
  
  // Detect first day of week from locale (US, Canada, Japan = Sunday; most others = Monday)
  const sundayCountries = ['US', 'CA', 'JP', 'AU', 'NZ', 'IL', 'PH', 'TW'];
  const locale = navigator.language || 'en-US';
  const country = locale.split('-')[1]?.toUpperCase() || 'US';
  detected.firstDayOfWeek = sundayCountries.includes(country) ? 'sunday' : 'monday';
  
  // Detect temperature unit (US, Bahamas, Cayman, Liberia, Palau = Fahrenheit)
  const fahrenheitCountries = ['US', 'BS', 'KY', 'LR', 'PW'];
  detected.temperatureUnit = fahrenheitCountries.includes(country) ? 'fahrenheit' : 'celsius';
  
  // Detect currency from locale
  const currencyMap: Record<string, UserPreferences['currency']> = {
    'US': 'USD', 'CA': 'CAD', 'AU': 'AUD', 'GB': 'GBP', 'UK': 'GBP',
    'DE': 'EUR', 'FR': 'EUR', 'ES': 'EUR', 'IT': 'EUR', 'NL': 'EUR',
    'JP': 'JPY', 'CH': 'CHF', 'IL': 'ILS',
  };
  detected.currency = currencyMap[country] || 'USD';
  
  return detected;
};

export const useStore = create<BrewOSState>()(
  subscribeWithSelector((set, get) => ({
    // Initial state
    connectionState: "disconnected",
    device: defaultDevice,
    machine: defaultMachine,
    temps: defaultTemps,
    pressure: 0,
    power: defaultPower,
    cleaning: defaultCleaning,
    water: defaultWater,
    scale: defaultScale,
    scaleScanning: false,
    scanResults: [],
    bbw: defaultBBW,
    shot: defaultShot,
    preinfusion: defaultPreinfusion,
    ecoMode: defaultEcoMode,
    wifi: defaultWifi,
    mqtt: defaultMqtt,
    esp32: defaultEsp32,
    pico: defaultPico,
    stats: defaultStats,
    alerts: [],
    logs: [],
    diagnostics: defaultDiagnostics,
    preferences: loadPreferencesFromStorage(),

    // Actions
    setConnectionState: (state) => set({ connectionState: state }),

    processMessage: (message) => {
      const { type, ...data } = message;

      switch (type) {
        // =======================================================================
        // Unified Status - Primary message type (comprehensive machine state)
        // =======================================================================
        case "status": {
          const machineData = data.machine as
            | Record<string, unknown>
            | undefined;
          const tempsData = data.temps as Record<string, unknown> | undefined;
          const powerData = data.power as Record<string, unknown> | undefined;
          const cleaningData = data.cleaning as
            | Record<string, unknown>
            | undefined;
          const waterData = data.water as Record<string, unknown> | undefined;
          const scaleData = data.scale as Record<string, unknown> | undefined;
          const connectionsData = data.connections as
            | Record<string, unknown>
            | undefined;
          const wifiData = data.wifi as Record<string, unknown> | undefined;
          const mqttData = data.mqtt as Record<string, unknown> | undefined;
          const esp32Data = data.esp32 as Record<string, unknown> | undefined;
          const shotData = data.shot as Record<string, unknown> | undefined;
          const statsData = data.stats as Record<string, unknown> | undefined;

          set((state) => ({
            // Machine state
            machine: machineData
              ? {
                  ...state.machine,
                  state:
                    (machineData.state as MachineStatus["state"]) ||
                    state.machine.state,
                  mode:
                    (machineData.mode as MachineStatus["mode"]) ||
                    state.machine.mode,
                  isHeating:
                    (machineData.isHeating as boolean) ??
                    state.machine.isHeating,
                  isBrewing:
                    (machineData.isBrewing as boolean) ??
                    state.machine.isBrewing,
                  heatingStrategy:
                    machineData.heatingStrategy !== undefined
                      ? (machineData.heatingStrategy as MachineStatus["heatingStrategy"])
                      : state.machine.heatingStrategy,
                  machineOnTimestamp:
                    machineData.machineOnTimestamp !== undefined
                      ? (machineData.machineOnTimestamp as number | null)
                      : state.machine.machineOnTimestamp,
                  lastShotTimestamp:
                    machineData.lastShotTimestamp !== undefined
                      ? (machineData.lastShotTimestamp as number | null)
                      : state.machine.lastShotTimestamp,
                }
              : state.machine,
            // Temperatures
            temps: tempsData
              ? {
                  brew: {
                    current:
                      ((tempsData.brew as Record<string, unknown>)
                        ?.current as number) ?? state.temps.brew.current,
                    setpoint:
                      ((tempsData.brew as Record<string, unknown>)
                        ?.setpoint as number) ?? state.temps.brew.setpoint,
                    max: state.temps.brew.max,
                  },
                  steam: {
                    current:
                      ((tempsData.steam as Record<string, unknown>)
                        ?.current as number) ?? state.temps.steam.current,
                    setpoint:
                      ((tempsData.steam as Record<string, unknown>)
                        ?.setpoint as number) ?? state.temps.steam.setpoint,
                    max: state.temps.steam.max,
                  },
                  group: (tempsData.group as number) ?? state.temps.group,
                }
              : state.temps,
            // Pressure
            pressure: (data.pressure as number) ?? state.pressure,
            // Power
            power: powerData
              ? {
                  ...state.power,
                  current: (powerData.current as number) ?? state.power.current,
                  voltage: (powerData.voltage as number) ?? state.power.voltage,
                  maxCurrent:
                    (powerData.maxCurrent as number) ?? state.power.maxCurrent,
                }
              : state.power,
            // Cleaning
            cleaning: cleaningData
              ? {
                  brewCount:
                    (cleaningData.brewCount as number) ??
                    state.cleaning.brewCount,
                  reminderDue:
                    (cleaningData.reminderDue as boolean) ??
                    state.cleaning.reminderDue,
                }
              : state.cleaning,
            // Water
            water: waterData
              ? {
                  tankLevel:
                    (waterData.tankLevel as WaterStatus["tankLevel"]) ||
                    state.water.tankLevel,
                }
              : state.water,
            // Scale
            scale: scaleData
              ? {
                  connected:
                    (scaleData.connected as boolean) ?? state.scale.connected,
                  name: (scaleData.name as string) || state.scale.name,
                  type: (scaleData.scaleType as string) || state.scale.type,
                  weight: (scaleData.weight as number) ?? state.scale.weight,
                  flowRate:
                    (scaleData.flowRate as number) ?? state.scale.flowRate,
                  stable: (scaleData.stable as boolean) ?? state.scale.stable,
                  battery: (scaleData.battery as number) ?? state.scale.battery,
                }
              : state.scale,
            // Shot (active brew data)
            shot: shotData
              ? {
                  active: (shotData.active as boolean) ?? state.shot.active,
                  startTime:
                    (shotData.startTime as number) ?? state.shot.startTime,
                  duration:
                    (shotData.duration as number) ?? state.shot.duration,
                  weight: (shotData.weight as number) ?? state.shot.weight,
                  flowRate:
                    (shotData.flowRate as number) ?? state.shot.flowRate,
                }
              : state.shot,
            // Connection status and full WiFi/MQTT data
            wifi: wifiData
              ? {
                  ...state.wifi,
                  ...(wifiData as Partial<WiFiStatus>),
                }
              : connectionsData
              ? {
                  ...state.wifi,
                  connected:
                    (connectionsData.wifi as boolean) ?? state.wifi.connected,
                }
              : state.wifi,
            mqtt: mqttData
              ? {
                  ...state.mqtt,
                  ...(mqttData as Partial<MQTTStatus>),
                }
              : connectionsData
              ? {
                  ...state.mqtt,
                  connected:
                    (connectionsData.mqtt as boolean) ?? state.mqtt.connected,
                }
              : state.mqtt,
            pico: connectionsData
              ? {
                  ...state.pico,
                  connected:
                    (connectionsData.pico as boolean) ?? state.pico.connected,
                }
              : state.pico,
            // ESP32 info
            esp32: esp32Data
              ? {
                  version: (esp32Data.version as string) || state.esp32.version,
                  freeHeap:
                    (esp32Data.freeHeap as number) ?? state.esp32.freeHeap,
                }
              : state.esp32,
            // Stats from status message (real-time updates)
            stats: statsData
              ? {
                  ...state.stats,
                  shotsToday:
                    (statsData.shotsToday as number) ?? state.stats.shotsToday,
                  sessionShots:
                    (statsData.sessionShots as number) ??
                    state.stats.sessionShots,
                  daily: statsData.daily
                    ? {
                        ...state.stats.daily,
                        shotCount:
                          ((statsData.daily as Record<string, unknown>)
                            .shotCount as number) ?? state.stats.daily.shotCount,
                        avgBrewTimeMs:
                          ((statsData.daily as Record<string, unknown>)
                            .avgBrewTimeMs as number) ??
                          state.stats.daily.avgBrewTimeMs,
                        totalKwh:
                          ((statsData.daily as Record<string, unknown>)
                            .totalKwh as number) ?? state.stats.daily.totalKwh,
                      }
                    : state.stats.daily,
                  lifetime: statsData.lifetime
                    ? {
                        ...state.stats.lifetime,
                        totalShots:
                          ((statsData.lifetime as Record<string, unknown>)
                            .totalShots as number) ??
                          state.stats.lifetime.totalShots,
                        avgBrewTimeMs:
                          ((statsData.lifetime as Record<string, unknown>)
                            .avgBrewTimeMs as number) ??
                          state.stats.lifetime.avgBrewTimeMs,
                        totalKwh:
                          ((statsData.lifetime as Record<string, unknown>)
                            .totalKwh as number) ??
                          state.stats.lifetime.totalKwh,
                      }
                    : state.stats.lifetime,
                }
              : state.stats,
          }));
          break;
        }

        // =======================================================================
        // Legacy message types - kept for backwards compatibility
        // =======================================================================
        case "esp_status":
          set((state) => ({
            esp32: {
              version: (data.version as string) || state.esp32.version,
              freeHeap: (data.freeHeap as number) ?? state.esp32.freeHeap,
            },
            wifi: data.wifi
              ? {
                  ...state.wifi,
                  ...(data.wifi as Partial<WiFiStatus>),
                }
              : state.wifi,
            mqtt: data.mqtt
              ? {
                  ...state.mqtt,
                  ...(data.mqtt as Partial<MQTTStatus>),
                }
              : state.mqtt,
          }));
          break;

        case "pico_status":
          set((state) => ({
            pico: {
              connected: true,
              version: (data.version as string) || state.pico.version,
            },
            machine: {
              ...state.machine,
              state:
                (data.state as MachineStatus["state"]) || state.machine.state,
              mode: (data.mode as MachineStatus["mode"]) || state.machine.mode,
              isHeating: (data.isHeating as boolean) ?? state.machine.isHeating,
              isBrewing: (data.isBrewing as boolean) ?? state.machine.isBrewing,
              machineOnTimestamp:
                data.machineOnTimestamp !== undefined
                  ? (data.machineOnTimestamp as number | null)
                  : state.machine.machineOnTimestamp,
              heatingStrategy:
                data.heatingStrategy !== undefined
                  ? (data.heatingStrategy as MachineStatus["heatingStrategy"])
                  : state.machine.heatingStrategy,
              lastShotTimestamp:
                data.lastShotTimestamp !== undefined
                  ? (data.lastShotTimestamp as number | null)
                  : state.machine.lastShotTimestamp,
            },
            temps: {
              brew: {
                current: (data.brewTemp as number) ?? state.temps.brew.current,
                setpoint:
                  (data.brewSetpoint as number) ?? state.temps.brew.setpoint,
                max: state.temps.brew.max,
              },
              steam: {
                current:
                  (data.steamTemp as number) ?? state.temps.steam.current,
                setpoint:
                  (data.steamSetpoint as number) ?? state.temps.steam.setpoint,
                max: state.temps.steam.max,
              },
              group: (data.groupTemp as number) ?? state.temps.group,
            },
            pressure: (data.pressure as number) ?? state.pressure,
            power: {
              ...state.power,
              current: (data.power as number) ?? state.power.current,
              voltage: (data.voltage as number) ?? state.power.voltage,
            },
            water: {
              tankLevel:
                (data.waterLevel as WaterStatus["tankLevel"]) ||
                state.water.tankLevel,
            },
          }));
          break;

        case "scale_status":
          set((state) => ({
            scale: {
              connected: (data.connected as boolean) ?? state.scale.connected,
              name: (data.name as string) || state.scale.name,
              type: (data.scaleType as string) || state.scale.type,
              weight: (data.weight as number) ?? state.scale.weight,
              flowRate: (data.flowRate as number) ?? state.scale.flowRate,
              stable: (data.stable as boolean) ?? state.scale.stable,
              battery: (data.battery as number) ?? state.scale.battery,
            },
          }));
          break;

        case "bbw_settings":
          set((state) => ({
            bbw: {
              enabled: (data.enabled as boolean) ?? state.bbw.enabled,
              targetWeight: (data.targetWeight as number) ?? state.bbw.targetWeight,
              doseWeight: (data.doseWeight as number) ?? state.bbw.doseWeight,
              stopOffset: (data.stopOffset as number) ?? state.bbw.stopOffset,
              autoTare: (data.autoTare as boolean) ?? state.bbw.autoTare,
            },
          }));
          break;

        case "preinfusion_settings":
          set((state) => ({
            preinfusion: {
              enabled: (data.enabled as boolean) ?? state.preinfusion.enabled,
              onTimeMs: (data.onTimeMs as number) ?? state.preinfusion.onTimeMs,
              pauseTimeMs: (data.pauseTimeMs as number) ?? state.preinfusion.pauseTimeMs,
            },
          }));
          break;

        case "stats": {
          const lifetimeData = data.lifetime as
            | Partial<LifetimeStats>
            | undefined;
          const dailyData = data.daily as Partial<PeriodStats> | undefined;
          const weeklyData = data.weekly as Partial<PeriodStats> | undefined;
          const monthlyData = data.monthly as Partial<PeriodStats> | undefined;
          const maintData = data.maintenance as
            | Partial<MaintenanceStats>
            | undefined;

          set((state) => {
            // Build new stats object
            const newStats: Statistics = {
              ...state.stats,
              // Enhanced structure
              lifetime: lifetimeData
                ? {
                    totalShots:
                      lifetimeData.totalShots ??
                      state.stats.lifetime.totalShots,
                    totalSteamCycles:
                      lifetimeData.totalSteamCycles ??
                      state.stats.lifetime.totalSteamCycles,
                    totalKwh:
                      lifetimeData.totalKwh ?? state.stats.lifetime.totalKwh,
                    totalOnTimeMinutes:
                      lifetimeData.totalOnTimeMinutes ??
                      state.stats.lifetime.totalOnTimeMinutes,
                    totalBrewTimeMs:
                      lifetimeData.totalBrewTimeMs ??
                      state.stats.lifetime.totalBrewTimeMs,
                    avgBrewTimeMs:
                      lifetimeData.avgBrewTimeMs ??
                      state.stats.lifetime.avgBrewTimeMs,
                    minBrewTimeMs:
                      lifetimeData.minBrewTimeMs ??
                      state.stats.lifetime.minBrewTimeMs,
                    maxBrewTimeMs:
                      lifetimeData.maxBrewTimeMs ??
                      state.stats.lifetime.maxBrewTimeMs,
                    firstShotTimestamp:
                      lifetimeData.firstShotTimestamp ??
                      state.stats.lifetime.firstShotTimestamp,
                  }
                : state.stats.lifetime,
              daily: dailyData
                ? {
                    shotCount:
                      dailyData.shotCount ?? state.stats.daily.shotCount,
                    totalBrewTimeMs:
                      dailyData.totalBrewTimeMs ??
                      state.stats.daily.totalBrewTimeMs,
                    avgBrewTimeMs:
                      dailyData.avgBrewTimeMs ??
                      state.stats.daily.avgBrewTimeMs,
                    minBrewTimeMs:
                      dailyData.minBrewTimeMs ??
                      state.stats.daily.minBrewTimeMs,
                    maxBrewTimeMs:
                      dailyData.maxBrewTimeMs ??
                      state.stats.daily.maxBrewTimeMs,
                    totalKwh: dailyData.totalKwh ?? state.stats.daily.totalKwh,
                  }
                : state.stats.daily,
              weekly: weeklyData
                ? {
                    shotCount:
                      weeklyData.shotCount ?? state.stats.weekly.shotCount,
                    totalBrewTimeMs:
                      weeklyData.totalBrewTimeMs ??
                      state.stats.weekly.totalBrewTimeMs,
                    avgBrewTimeMs:
                      weeklyData.avgBrewTimeMs ??
                      state.stats.weekly.avgBrewTimeMs,
                    minBrewTimeMs:
                      weeklyData.minBrewTimeMs ??
                      state.stats.weekly.minBrewTimeMs,
                    maxBrewTimeMs:
                      weeklyData.maxBrewTimeMs ??
                      state.stats.weekly.maxBrewTimeMs,
                    totalKwh:
                      weeklyData.totalKwh ?? state.stats.weekly.totalKwh,
                  }
                : state.stats.weekly,
              monthly: monthlyData
                ? {
                    shotCount:
                      monthlyData.shotCount ?? state.stats.monthly.shotCount,
                    totalBrewTimeMs:
                      monthlyData.totalBrewTimeMs ??
                      state.stats.monthly.totalBrewTimeMs,
                    avgBrewTimeMs:
                      monthlyData.avgBrewTimeMs ??
                      state.stats.monthly.avgBrewTimeMs,
                    minBrewTimeMs:
                      monthlyData.minBrewTimeMs ??
                      state.stats.monthly.minBrewTimeMs,
                    maxBrewTimeMs:
                      monthlyData.maxBrewTimeMs ??
                      state.stats.monthly.maxBrewTimeMs,
                    totalKwh:
                      monthlyData.totalKwh ?? state.stats.monthly.totalKwh,
                  }
                : state.stats.monthly,
              maintenance: maintData
                ? {
                    shotsSinceBackflush:
                      maintData.shotsSinceBackflush ??
                      state.stats.maintenance.shotsSinceBackflush,
                    shotsSinceGroupClean:
                      maintData.shotsSinceGroupClean ??
                      state.stats.maintenance.shotsSinceGroupClean,
                    shotsSinceDescale:
                      maintData.shotsSinceDescale ??
                      state.stats.maintenance.shotsSinceDescale,
                    lastBackflushTimestamp:
                      maintData.lastBackflushTimestamp ??
                      state.stats.maintenance.lastBackflushTimestamp,
                    lastGroupCleanTimestamp:
                      maintData.lastGroupCleanTimestamp ??
                      state.stats.maintenance.lastGroupCleanTimestamp,
                    lastDescaleTimestamp:
                      maintData.lastDescaleTimestamp ??
                      state.stats.maintenance.lastDescaleTimestamp,
                  }
                : state.stats.maintenance,
              sessionShots:
                (data.sessionShots as number) ?? state.stats.sessionShots,
              sessionStartTimestamp:
                (data.sessionStartTimestamp as number) ??
                state.stats.sessionStartTimestamp,
            };

            // Update legacy compatibility fields
            newStats.totalShots = newStats.lifetime.totalShots;
            newStats.totalSteamCycles = newStats.lifetime.totalSteamCycles;
            newStats.totalKwh = newStats.lifetime.totalKwh;
            newStats.totalOnTimeMinutes = newStats.lifetime.totalOnTimeMinutes;
            newStats.avgBrewTimeMs = newStats.lifetime.avgBrewTimeMs;
            newStats.minBrewTimeMs = newStats.lifetime.minBrewTimeMs;
            newStats.maxBrewTimeMs = newStats.lifetime.maxBrewTimeMs;
            newStats.shotsToday = newStats.daily.shotCount;
            newStats.kwhToday = newStats.daily.totalKwh;
            newStats.dailyCount = newStats.daily.shotCount;
            newStats.weeklyCount = newStats.weekly.shotCount;
            newStats.monthlyCount = newStats.monthly.shotCount;
            newStats.shotsSinceBackflush =
              newStats.maintenance.shotsSinceBackflush;
            newStats.shotsSinceGroupClean =
              newStats.maintenance.shotsSinceGroupClean;
            newStats.shotsSinceDescale = newStats.maintenance.shotsSinceDescale;
            newStats.lastBackflushTimestamp =
              newStats.maintenance.lastBackflushTimestamp;
            newStats.lastGroupCleanTimestamp =
              newStats.maintenance.lastGroupCleanTimestamp;
            newStats.lastDescaleTimestamp =
              newStats.maintenance.lastDescaleTimestamp;

            return { stats: newStats };
          });
          break;
        }

        case "scan_result":
          get().addScanResult(data as unknown as ScaleScanResult);
          break;

        case "scan_complete":
          set({ scaleScanning: false });
          break;

        case "event":
          handleEvent(data, set, get);
          break;

        case "log":
          addLog(data, set, get);
          break;

        case "error":
          addAlert("error", data.message as string, set, get);
          break;

        case "device_info": {
          // Process preferences from ESP32 if provided
          const prefsData = data.preferences as Record<string, unknown> | undefined;
          const prefsInitialized = prefsData?.initialized as boolean | undefined;
          
          set((state) => ({
            device: {
              deviceId: (data.deviceId as string) || state.device.deviceId,
              deviceName:
                (data.deviceName as string) || state.device.deviceName,
              machineBrand:
                (data.machineBrand as string) || state.device.machineBrand,
              machineModel:
                (data.machineModel as string) || state.device.machineModel,
              machineType:
                (data.machineType as DeviceInfo["machineType"]) ||
                state.device.machineType,
              firmwareVersion:
                (data.firmwareVersion as string) ||
                state.device.firmwareVersion,
              hasPressureSensor:
                data.hasPressureSensor !== undefined
                  ? (data.hasPressureSensor as boolean)
                  : state.device.hasPressureSensor,
            },
            // Update power settings if provided
            power: {
              ...state.power,
              voltage:
                (data.mainsVoltage as number) ?? state.power.voltage,
              maxCurrent:
                (data.maxCurrent as number) ?? state.power.maxCurrent,
            },
            // Update eco mode settings if provided
            ecoMode: {
              ...(state.ecoMode ?? defaultEcoMode),
              ecoBrewTemp:
                (data.ecoBrewTemp as number) ?? state.ecoMode.ecoBrewTemp,
              autoOffTimeout:
                (data.ecoTimeoutMinutes as number) ?? state.ecoMode.autoOffTimeout,
            },
            // Update pre-infusion settings if provided
            preinfusion: {
              ...(state.preinfusion ?? defaultPreinfusion),
              enabled:
                (data.preinfusionEnabled as boolean) ?? state.preinfusion.enabled,
              onTimeMs:
                (data.preinfusionOnMs as number) ?? state.preinfusion.onTimeMs,
              pauseTimeMs:
                (data.preinfusionPauseMs as number) ?? state.preinfusion.pauseTimeMs,
            },
            // Update preferences from ESP32 (synced across devices)
            preferences: prefsData
              ? {
                  firstDayOfWeek:
                    (prefsData.firstDayOfWeek as UserPreferences["firstDayOfWeek"]) ??
                    state.preferences.firstDayOfWeek,
                  use24HourTime:
                    (prefsData.use24HourTime as boolean) ?? state.preferences.use24HourTime,
                  temperatureUnit:
                    (prefsData.temperatureUnit as UserPreferences["temperatureUnit"]) ??
                    state.preferences.temperatureUnit,
                  electricityPrice:
                    (prefsData.electricityPrice as number) ?? state.preferences.electricityPrice,
                  currency:
                    (prefsData.currency as UserPreferences["currency"]) ?? state.preferences.currency,
                }
              : state.preferences,
          }));
          
          // Also cache preferences locally
          if (prefsData) {
            const state = get();
            savePreferencesToStorage(state.preferences);
          }
          
          // If preferences not initialized on ESP32, send browser-detected defaults
          // This is dispatched as a custom event to be handled by the connection manager
          if (prefsData && prefsInitialized === false) {
            const browserPrefs = detectBrowserPreferences();
            window.dispatchEvent(new CustomEvent('brewos:init-preferences', { 
              detail: { ...defaultPreferences, ...browserPrefs }
            }));
          }
          break;
        }

        case "power_meter_status": {
          const meterData = data as Record<string, unknown>;
          const readingData = meterData.reading as Record<string, unknown> | null;
          
          set((state) => ({
            power: {
              ...state.power,
              meter: {
                source: (meterData.source as PowerMeterStatus["source"]) || "none",
                connected: (meterData.connected as boolean) ?? false,
                meterType: (meterData.meterType as string) || null,
                lastUpdate: (meterData.lastUpdate as number) || null,
                reading: readingData ? {
                  voltage: (readingData.voltage as number) ?? 0,
                  current: (readingData.current as number) ?? 0,
                  power: (readingData.power as number) ?? 0,
                  energy: (readingData.energy as number) ?? 0,
                  frequency: (readingData.frequency as number) ?? 0,
                  powerFactor: (readingData.powerFactor as number) ?? 0,
                } : null,
                error: (meterData.error as string) || null,
                discovering: (meterData.discovering as boolean) ?? false,
                discoveryProgress: (meterData.discoveryProgress as string) || undefined,
                discoveryStep: (meterData.discoveryStep as number) || undefined,
                discoveryTotal: (meterData.discoveryTotal as number) || undefined,
              },
            },
          }));
          break;
        }

        case "diagnostics_header": {
          const header: DiagnosticHeader = {
            testCount: (data.testCount as number) ?? 0,
            passCount: (data.passCount as number) ?? 0,
            failCount: (data.failCount as number) ?? 0,
            warnCount: (data.warnCount as number) ?? 0,
            skipCount: (data.skipCount as number) ?? 0,
            isComplete: (data.isComplete as boolean) ?? false,
            durationMs: (data.durationMs as number) ?? 0,
          };
          set((state) => ({
            diagnostics: {
              ...state.diagnostics,
              header,
              isRunning: !header.isComplete,
              timestamp: header.isComplete
                ? Date.now()
                : state.diagnostics.timestamp,
              // Clear results on new test run (when first header arrives)
              results: state.diagnostics.isRunning
                ? state.diagnostics.results
                : [],
            },
          }));
          break;
        }

        case "diagnostics_result": {
          const testIdNum = (data.testId as number) ?? 0;
          const result: DiagnosticResult = {
            testId: testIdNum as DiagnosticTestId,
            name: getDiagnosticTestName(testIdNum),
            status: diagStatusFromCode((data.status as number) ?? 1),
            rawValue: (data.rawValue as number) ?? 0,
            expectedMin: (data.expectedMin as number) ?? 0,
            expectedMax: (data.expectedMax as number) ?? 0,
            message: (data.message as string) ?? "",
          };
          set((state) => ({
            diagnostics: {
              ...state.diagnostics,
              results: [...state.diagnostics.results, result],
            },
          }));
          break;
        }
      }
    },

    dismissAlert: (id) =>
      set((state) => ({
        alerts: state.alerts.map((a) =>
          a.id === id ? { ...a, dismissed: true } : a
        ),
      })),

    clearLogs: () => set({ logs: [] }),

    addScanResult: (result) =>
      set((state) => ({
        scanResults: [...state.scanResults, result],
      })),

    setScaleScanning: (scanning) => set({ scaleScanning: scanning }),

    clearScanResults: () => set({ scanResults: [] }),

    setPreference: (key, value) => {
      set((state) => {
        const newPrefs = { ...state.preferences, [key]: value };
        // Cache locally for offline use
        savePreferencesToStorage(newPrefs);
        return { preferences: newPrefs };
      });
      
      // Note: The actual save to ESP32 happens via sendCommand('set_preferences', ...)
      // This is handled in the UI components that call setPreference
    },

    setDiagnosticsRunning: (running) => {
      set((state) => ({
        diagnostics: {
          ...state.diagnostics,
          isRunning: running,
          // Clear results when starting new test
          results: running ? [] : state.diagnostics.results,
          timestamp: running ? 0 : state.diagnostics.timestamp,
        },
      }));
    },

    resetDiagnostics: () => {
      set({ diagnostics: defaultDiagnostics });
    },
  }))
);

// Helper functions
type SetState = (
  partial: Partial<BrewOSState> | ((state: BrewOSState) => Partial<BrewOSState>)
) => void;

function handleEvent(
  data: Record<string, unknown>,
  set: SetState,
  get: () => BrewOSState
) {
  switch (data.event) {
    case "shot_start":
      set({
        shot: {
          active: true,
          startTime: Date.now(),
          duration: 0,
          weight: 0,
          flowRate: 0,
        },
      });
      break;

    case "shot_end": {
      const state = get();
      set({
        shot: { ...state.shot, active: false },
        machine: {
          ...state.machine,
          lastShotTimestamp: Date.now(),
        },
        stats: {
          ...state.stats,
          totalShots: state.stats.totalShots + 1,
          shotsToday: state.stats.shotsToday + 1,
          sessionShots: state.stats.sessionShots + 1,
          shotsSinceDescale: state.stats.shotsSinceDescale + 1,
          shotsSinceGroupClean: state.stats.shotsSinceGroupClean + 1,
          shotsSinceBackflush: state.stats.shotsSinceBackflush + 1,
        },
      });
      break;
    }

    case "alert":
      addAlert(
        (data.level as Alert["level"]) || "warning",
        data.message as string,
        set,
        get
      );
      break;
  }
}

function addLog(
  data: Record<string, unknown>,
  set: SetState,
  get: () => BrewOSState
) {
  const log: LogEntry = {
    id: Date.now(),
    time: new Date().toISOString(),
    level: (data.level as string) || "info",
    message: data.message as string,
  };
  set({ logs: [log, ...get().logs.slice(0, 99)] });
}

function addAlert(
  level: Alert["level"],
  message: string,
  set: SetState,
  get: () => BrewOSState
) {
  const alert: Alert = {
    id: Date.now(),
    time: new Date().toISOString(),
    level,
    message,
    dismissed: false,
  };
  set({ alerts: [alert, ...get().alerts] });
}

// Connection helper
export function initializeStore(connection: IConnection) {
  connection.onStateChange((state) => {
    useStore.getState().setConnectionState(state);
  });

  connection.onMessage((message) => {
    useStore.getState().processMessage(message);
  });
}
