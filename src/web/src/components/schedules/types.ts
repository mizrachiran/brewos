export interface Schedule {
  id: number;
  enabled: boolean;
  days: number;
  hour: number;
  minute: number;
  action: "on" | "off";
  strategy: number;
  name: string;
}

export interface ScheduleFormData {
  enabled: boolean;
  days: number;
  hour: number;
  minute: number;
  action: "on" | "off";
  strategy: number;
  name: string;
}

export interface DayInfo {
  value: number;
  label: string;
  short: string;
  index: number;
}

// Days of week - starting from Sunday (standard ISO)
export const ALL_DAYS: DayInfo[] = [
  { value: 0x01, label: "Sun", short: "S", index: 0 },
  { value: 0x02, label: "Mon", short: "M", index: 1 },
  { value: 0x04, label: "Tue", short: "T", index: 2 },
  { value: 0x08, label: "Wed", short: "W", index: 3 },
  { value: 0x10, label: "Thu", short: "T", index: 4 },
  { value: 0x20, label: "Fri", short: "F", index: 5 },
  { value: 0x40, label: "Sat", short: "S", index: 6 },
];

export const WEEKDAYS = 0x3e; // Mon-Fri
export const WEEKENDS = 0x41; // Sat-Sun
export const EVERY_DAY = 0x7f;

export const STRATEGIES = [
  { value: 0, label: "Brew Only", desc: "Heat only brew boiler" },
  { value: 1, label: "Sequential", desc: "Brew first, then steam" },
  { value: 2, label: "Parallel", desc: "Heat both simultaneously" },
  { value: 3, label: "Smart Stagger", desc: "Power-aware heating" },
];

export const DEFAULT_SCHEDULE: ScheduleFormData = {
  enabled: true,
  days: WEEKDAYS,
  hour: 7,
  minute: 0,
  action: "on",
  strategy: 1,
  name: "",
};

// Helper to reorder days based on first day of week
export const getOrderedDays = (firstDay: "sunday" | "monday"): DayInfo[] => {
  if (firstDay === "monday") {
    return [...ALL_DAYS.slice(1), ALL_DAYS[0]];
  }
  return ALL_DAYS;
};

