// Connection types
export type ConnectionState =
  | "disconnected"
  | "connecting"
  | "connected"
  | "reconnecting"
  | "error";

export type ConnectionMode = "local" | "cloud" | "demo";

export interface ConnectionConfig {
  mode: ConnectionMode;
  endpoint?: string;
  cloudUrl?: string;
  authToken?: string;
  deviceId?: string;
}

// Common interface for connection classes (real and demo)
export interface IConnection {
  connect(): Promise<void>;
  disconnect(): void;
  send(type: string, payload?: Record<string, unknown>): void;
  sendCommand(cmd: string, data?: Record<string, unknown>): void;
  onMessage(handler: (message: WebSocketMessage) => void): () => void;
  onStateChange(handler: (state: ConnectionState) => void): () => void;
  getState(): ConnectionState;
}

// Message types from ESP32
export type MessageType =
  | "status"
  | "esp_status"
  | "pico_status"
  | "scale_status"
  | "power_meter_status"
  | "bbw_settings"
  | "preinfusion_settings"
  | "stats"
  | "event"
  | "log"
  | "error"
  | "scan_result"
  | "scan_complete"
  | "device_info"
  | "diagnostics_header"
  | "diagnostics_result";

export interface WebSocketMessage {
  type: MessageType;
  [key: string]: unknown;
}

// Machine state
export type MachineState =
  | "unknown"
  | "init"
  | "idle"
  | "heating"
  | "ready"
  | "brewing"
  | "steaming"
  | "cooldown"
  | "fault";

export type MachineMode = "standby" | "on" | "eco";

export type HeatingStrategy = 0 | 1 | 2 | 3; // 0=Brew Only, 1=Sequential, 2=Parallel, 3=Smart Stagger

export interface MachineStatus {
  state: MachineState;
  mode: MachineMode;
  isHeating: boolean;
  isBrewing: boolean;
  machineOnTimestamp: number | null; // Unix timestamp (ms) when machine was turned ON, null if off
  heatingStrategy: HeatingStrategy | null; // Active heating strategy when machine is on
  lastShotTimestamp: number | null; // Unix timestamp (ms) of last completed shot
}

// Temperature
export interface BoilerTemp {
  current: number;
  setpoint: number;
  max: number;
}

export interface Temperatures {
  brew: BoilerTemp;
  steam: BoilerTemp;
  group: number; // Group head temp (°C) - kept for protocol compatibility
}

// Power Meter
export interface PowerMeterReading {
  voltage: number;
  current: number;
  power: number;
  energy: number;
  frequency: number;
  powerFactor: number;
}

export interface PowerMeterStatus {
  source: "none" | "hardware" | "mqtt";
  connected: boolean;
  meterType: string | null;
  lastUpdate: number | null;
  reading: PowerMeterReading | null;
  error: string | null;
  discovering?: boolean;
  discoveryProgress?: string;
  discoveryStep?: number;
  discoveryTotal?: number;
}

// Power
export interface PowerStatus {
  current: number;
  voltage: number;
  maxCurrent: number; // Max current limit (Amps) - from device settings
  todayKwh: number;
  totalKwh: number;
  meter?: PowerMeterStatus; // Optional power meter status
}

// Cleaning
export interface CleaningStatus {
  brewCount: number; // Brews since last cleaning
  reminderDue: boolean; // True when cleaning is recommended
}

// Water
export type WaterLevel = "ok" | "low" | "empty";

export interface WaterStatus {
  tankLevel: WaterLevel;
}

// Scale
export interface ScaleStatus {
  connected: boolean;
  name: string;
  type: string;
  weight: number;
  flowRate: number;
  stable: boolean;
  battery: number;
}

export interface ScaleScanResult {
  address: string;
  name: string;
  rssi: number;
  type?: string;
}

// Brew-by-weight
export interface BBWSettings {
  enabled: boolean;
  targetWeight: number;
  doseWeight: number;
  stopOffset: number;
  autoTare: boolean;
}

// Pre-infusion
export interface PreinfusionSettings {
  enabled: boolean;
  onTimeMs: number; // Pump ON time in milliseconds
  pauseTimeMs: number; // Soak/pause time in milliseconds
}

// Shot
export interface ShotStatus {
  active: boolean;
  startTime: number;
  duration: number;
  weight: number;
  flowRate: number;
}

// WiFi
export interface WiFiStatus {
  connected: boolean;
  ssid: string;
  ip: string;
  rssi: number;
  apMode: boolean;
  // Static IP configuration
  staticIp: boolean;
  gateway: string;
  subnet: string;
  dns1: string;
  dns2: string;
}

// MQTT
export interface MQTTConfig {
  enabled: boolean;
  broker: string;
  port: number;
  username: string;
  password: string;
  topic: string;
  discovery: boolean;
}

export interface MQTTStatus {
  enabled: boolean;
  connected: boolean;
  broker: string;
  topic: string;
}

// Cloud
export interface CloudConfig {
  enabled: boolean;
  serverUrl: string;
  connected: boolean;
  deviceId: string;
}

// Machine type enum for type-safe comparisons
export type MachineType =
  | "dual_boiler"
  | "single_boiler"
  | "heat_exchanger"
  | "";

// Device info
export interface DeviceInfo {
  deviceId: string;
  deviceName: string;
  machineBrand: string;
  machineModel: string;
  machineType: MachineType; // dual_boiler, single_boiler, heat_exchanger
  firmwareVersion: string;
  // Optional hardware capabilities
  hasPressureSensor: boolean; // Pressure transducer installed (optional accessory)
}

export interface ESP32Info {
  version: string;
  freeHeap: number;
}

export interface PicoInfo {
  connected: boolean;
  version: string;
}

// =============================================================================
// STATISTICS TYPES - Enhanced statistics from ESP32
// =============================================================================

/**
 * Individual brew record with detailed metrics
 */
export interface BrewRecord {
  timestamp: number; // Unix timestamp
  durationMs: number; // Brew duration in milliseconds
  yieldWeight: number; // Output weight (g)
  doseWeight: number; // Input dose (g)
  peakPressure: number; // Maximum pressure during brew
  avgTemperature: number; // Average brew temperature
  avgFlowRate: number; // Average flow rate (g/s)
  rating: number; // User rating (0-5, 0=unrated)
  ratio?: number; // Brew ratio (yield/dose)
}

/**
 * Power consumption sample
 */
export interface PowerSample {
  timestamp: number; // Unix timestamp
  avgWatts: number; // Average power during interval
  maxWatts: number; // Peak power during interval
  kwhConsumed: number; // Energy consumed during interval
}

/**
 * Daily summary for trend analysis
 */
export interface DailySummary {
  date: number; // Unix timestamp at midnight
  shotCount: number; // Shots that day
  totalBrewTimeMs: number; // Total brew time
  totalKwh: number; // Total energy consumed
  onTimeMinutes: number; // Minutes machine was on
  steamCycles: number; // Steam cycle count
  avgBrewTimeMs: number; // Average brew time
}

/**
 * Statistics for a time period
 */
export interface PeriodStats {
  shotCount: number;
  totalBrewTimeMs: number;
  avgBrewTimeMs: number;
  minBrewTimeMs: number;
  maxBrewTimeMs: number;
  totalKwh: number;
}

/**
 * Lifetime statistics
 */
export interface LifetimeStats {
  totalShots: number;
  totalSteamCycles: number;
  totalKwh: number;
  totalOnTimeMinutes: number;
  totalBrewTimeMs: number;
  avgBrewTimeMs: number;
  minBrewTimeMs: number;
  maxBrewTimeMs: number;
  firstShotTimestamp: number;
}

/**
 * Maintenance tracking
 */
export interface MaintenanceStats {
  shotsSinceBackflush: number;
  shotsSinceGroupClean: number;
  shotsSinceDescale: number;
  lastBackflushTimestamp: number;
  lastGroupCleanTimestamp: number;
  lastDescaleTimestamp: number;
}

/**
 * Complete statistics package
 */
export interface Statistics {
  // Lifetime stats
  lifetime: LifetimeStats;

  // Period stats (calculated from history)
  daily: PeriodStats;
  weekly: PeriodStats;
  monthly: PeriodStats;

  // Maintenance
  maintenance: MaintenanceStats;

  // Session
  sessionShots: number;
  sessionStartTimestamp: number;

  // Legacy compatibility fields (derived from new structure)
  totalShots: number;
  totalSteamCycles: number;
  totalKwh: number;
  totalOnTimeMinutes: number;
  shotsToday: number;
  kwhToday: number;
  onTimeToday: number;
  shotsSinceDescale: number;
  shotsSinceGroupClean: number;
  shotsSinceBackflush: number;
  lastDescaleTimestamp: number;
  lastGroupCleanTimestamp: number;
  lastBackflushTimestamp: number;
  avgBrewTimeMs: number;
  minBrewTimeMs: number;
  maxBrewTimeMs: number;
  dailyCount: number;
  weeklyCount: number;
  monthlyCount: number;
}

/**
 * Extended statistics response (from /api/stats/extended)
 */
export interface ExtendedStatsResponse {
  stats: Statistics;
  weekly: { day: string; shots: number }[];
  hourlyDistribution: { hour: number; count: number }[];
  brewHistory: BrewRecord[];
  powerHistory: PowerSample[];
  dailyHistory: DailySummary[];
}

// =============================================================================
// DIAGNOSTICS TYPES - Hardware self-test and diagnostics
// =============================================================================

/**
 * Diagnostic test IDs - matches protocol_defs.h DIAG_TEST_*
 */
export type DiagnosticTestId =
  | 0x00 // ALL - run all tests
  | 0x01 // BREW_NTC
  | 0x02 // STEAM_NTC
  | 0x04 // PRESSURE
  | 0x05 // WATER_LEVEL
  | 0x06 // SSR_BREW
  | 0x07 // SSR_STEAM
  | 0x08 // RELAY_PUMP
  | 0x09 // RELAY_SOLENOID
  | 0x0a // POWER_METER
  | 0x0b // ESP32_COMM
  | 0x0c // BUZZER
  | 0x0d; // LED

/**
 * Diagnostic result status - matches protocol_defs.h DIAG_STATUS_*
 */
export type DiagnosticStatus = "pass" | "fail" | "warn" | "skip" | "running";

/**
 * Map status code to DiagnosticStatus
 */
export function diagStatusFromCode(code: number): DiagnosticStatus {
  switch (code) {
    case 0x00:
      return "pass";
    case 0x01:
      return "fail";
    case 0x02:
      return "warn";
    case 0x03:
      return "skip";
    case 0x04:
      return "running";
    default:
      return "fail";
  }
}

/**
 * Individual diagnostic test result
 */
export interface DiagnosticResult {
  testId: DiagnosticTestId;
  name: string; // Human-readable test name
  status: DiagnosticStatus;
  rawValue: number; // Raw sensor value
  expectedMin: number; // Expected minimum
  expectedMax: number; // Expected maximum
  message: string; // Result message
}

/**
 * Diagnostic report header/summary
 */
export interface DiagnosticHeader {
  testCount: number;
  passCount: number;
  failCount: number;
  warnCount: number;
  skipCount: number;
  isComplete: boolean;
  durationMs: number;
}

/**
 * Complete diagnostic report
 */
export interface DiagnosticReport {
  header: DiagnosticHeader;
  results: DiagnosticResult[];
  isRunning: boolean;
  timestamp: number;
}

/**
 * Diagnostic test metadata with machine type compatibility
 */
export interface DiagnosticTestMeta {
  id: number;
  name: string;
  description: string;
  /** Machine types this test applies to (empty = all types) */
  machineTypes: MachineType[];
  /** Whether this test is optional (user can skip/disable) */
  optional: boolean;
  /** Category for grouping in UI */
  category: "sensors" | "outputs" | "communication" | "peripheral";
}

/**
 * All diagnostic tests with metadata
 *
 * Based on ECM_Control_Board_Specification_v2.20:
 * - Section 3.1: Inputs (S1-S4 switches, T1-T3 temp sensors, P1 pressure)
 * - Section 3.2: Outputs (K1-K3 relays, SSR1-SSR2 heaters)
 * - Section 3.3: Communication (ESP32, PZEM)
 * - Section 3.4: User Interface (Status LED, Buzzer)
 */
export const DIAGNOSTIC_TESTS: DiagnosticTestMeta[] = [
  // ==========================================================================
  // TEMPERATURE SENSORS
  // ==========================================================================
  {
    id: 0x01,
    name: "Brew Boiler Temperature Sensor (NTC)",
    description: "T1: 50kΩ NTC thermistor on ADC0 (GPIO26) with 3.3kΩ pull-up",
    machineTypes: ["dual_boiler", "single_boiler"],
    optional: false,
    category: "sensors",
  },
  {
    id: 0x02,
    name: "Steam Boiler Temperature Sensor (NTC)",
    description: "T2: 50kΩ NTC thermistor on ADC1 (GPIO27) with 1.2kΩ pull-up",
    machineTypes: ["dual_boiler", "heat_exchanger"],
    optional: false,
    category: "sensors",
  },

  // ==========================================================================
  // PRESSURE SENSOR
  // ==========================================================================
  {
    id: 0x04,
    name: "Pump Pressure Transducer",
    description: "P1: YD4060 0-16bar, 0.5-4.5V on ADC2 (GPIO28)",
    machineTypes: [], // All machine types can optionally install
    optional: true, // Some users don't install pressure sensor
    category: "sensors",
  },

  // ==========================================================================
  // WATER LEVEL SENSORS (S1, S2, S3)
  // ==========================================================================
  {
    id: 0x05,
    name: "Water Reservoir & Tank Level Sensors",
    description: "S1: Reservoir switch (GPIO2), S2: Tank float sensor (GPIO3)",
    machineTypes: [], // All machine types
    optional: false,
    category: "sensors",
  },
  {
    id: 0x0e,
    name: "Steam Boiler Level Probe",
    description: "S3: Conductivity probe via OPA342/TLV3201 AC sensing (GPIO4)",
    machineTypes: ["dual_boiler", "heat_exchanger"], // Only machines with steam boilers
    optional: false,
    category: "sensors",
  },

  // ==========================================================================
  // BREW CONTROL INPUT
  // ==========================================================================
  {
    id: 0x0f,
    name: "Brew Lever/Handle Switch",
    description: "S4: Brew handle microswitch (GPIO5), active low",
    machineTypes: [], // All machine types
    optional: false,
    category: "sensors",
  },

  // ==========================================================================
  // HEATER OUTPUTS (SSRs)
  // ==========================================================================
  {
    id: 0x06,
    name: "Brew Heater (SSR)",
    description: "SSR1: Solid-state relay trigger on GPIO13",
    machineTypes: ["dual_boiler", "single_boiler"],
    optional: false,
    category: "outputs",
  },
  {
    id: 0x07,
    name: "Steam Heater (SSR)",
    description: "SSR2: Solid-state relay trigger on GPIO14",
    machineTypes: ["dual_boiler", "heat_exchanger"],
    optional: false,
    category: "outputs",
  },

  // ==========================================================================
  // RELAY OUTPUTS (K1, K2, K3)
  // ==========================================================================
  {
    id: 0x10,
    name: "Water Status LED Relay",
    description: "K1: Water indicator LED relay on GPIO10",
    machineTypes: [], // Machine-specific
    optional: true, // Not all machines have this indicator
    category: "outputs",
  },
  {
    id: 0x08,
    name: "Water Pump Relay",
    description: "K2: Ulka pump relay (16A) on GPIO11",
    machineTypes: [], // All machine types
    optional: false,
    category: "outputs",
  },
  {
    id: 0x09,
    name: "Brew Solenoid Valve Relay",
    description: "K3: 3-way solenoid valve relay on GPIO12",
    machineTypes: [], // All machine types
    optional: false,
    category: "outputs",
  },

  // ==========================================================================
  // COMMUNICATION
  // ==========================================================================
  {
    id: 0x0b,
    name: "ESP32 Communication",
    description: "UART0 link on GPIO0/1 (921600 baud)",
    machineTypes: [], // All machine types
    optional: false,
    category: "communication",
  },
  {
    id: 0x0a,
    name: "Hardware Power Meter",
    description: "Modbus RTU via UART1 on GPIO6/7 (PZEM, JSY, Eastron meters)",
    machineTypes: [], // All machine types
    optional: true, // Optional power monitoring add-on
    category: "communication",
  },

  // ==========================================================================
  // USER INTERFACE
  // ==========================================================================
  {
    id: 0x0c,
    name: "Buzzer / Piezo Speaker",
    description: "Passive piezo buzzer on GPIO19 (PWM)",
    machineTypes: [], // All machine types
    optional: false,
    category: "peripheral",
  },
  {
    id: 0x0d,
    name: "Status LED",
    description: "Green indicator LED on GPIO15",
    machineTypes: [], // All machine types
    optional: false,
    category: "peripheral",
  },
];

/**
 * Get tests applicable to a specific machine type
 */
export function getTestsForMachineType(
  machineType: MachineType
): DiagnosticTestMeta[] {
  return DIAGNOSTIC_TESTS.filter(
    (test) =>
      test.machineTypes.length === 0 || test.machineTypes.includes(machineType)
  );
}

/**
 * Get human-readable name for a diagnostic test
 */
export function getDiagnosticTestName(testId: number): string {
  const test = DIAGNOSTIC_TESTS.find((t) => t.id === testId);
  if (test) return test.name;

  // Fallback for test IDs not in DIAGNOSTIC_TESTS
  const fallbackNames: Record<number, string> = {
    0x00: "All Tests",
  };
  return (
    fallbackNames[testId] ||
    `Unknown Test (0x${testId.toString(16).padStart(2, "0").toUpperCase()})`
  );
}

// Alerts & Logs
export interface Alert {
  id: number;
  time: string;
  level: "info" | "warning" | "error";
  message: string;
  dismissed: boolean;
}

export interface LogEntry {
  id: number;
  time: string;
  level: string;
  message: string;
}

// Cloud device (from API)
export interface CloudDevice {
  id: string;
  name: string;
  isOnline: boolean;
  lastSeen: string | null;
  firmwareVersion: string | null;
  machineType: string | null;
  claimedAt: string;
  userCount?: number; // Number of users with access to this device
}

// User preferences (stored in localStorage)
export type FirstDayOfWeek = "sunday" | "monday";
export type TemperatureUnit = "celsius" | "fahrenheit";

export type Currency = 'USD' | 'EUR' | 'GBP' | 'AUD' | 'CAD' | 'JPY' | 'CHF' | 'ILS';

export interface UserPreferences {
  firstDayOfWeek: FirstDayOfWeek;
  use24HourTime: boolean;
  temperatureUnit: TemperatureUnit;
  // Electricity settings
  electricityPrice: number;  // Price per kWh in selected currency
  currency: Currency;
  // App badge (PWA only) - shows dot on app icon when machine is on
  showAppBadge: boolean;
}
