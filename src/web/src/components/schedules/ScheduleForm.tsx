import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Power, PowerOff, Save, X } from "lucide-react";
import {
  type ScheduleFormData,
  type DayInfo,
  STRATEGIES,
} from "./types";

interface ScheduleFormProps {
  data: ScheduleFormData;
  onChange: (data: ScheduleFormData) => void;
  onSave: () => void;
  onCancel: () => void;
  orderedDays: DayInfo[];
  isDualBoiler: boolean;
  saving?: boolean;
}

export function ScheduleForm({
  data,
  onChange,
  onSave,
  onCancel,
  orderedDays,
  isDualBoiler,
  saving,
}: ScheduleFormProps) {
  const toggleDay = (dayValue: number) => {
    onChange({ ...data, days: data.days ^ dayValue });
  };

  const setPresetDays = (preset: "weekdays" | "weekends" | "everyday") => {
    const days = preset === "weekdays" ? 0x3e : preset === "weekends" ? 0x41 : 0x7f;
    onChange({ ...data, days });
  };

  return (
    <div className="space-y-4">
      <Input
        label="Name (optional)"
        placeholder="Morning brew, Evening shutdown..."
        value={data.name}
        onChange={(e) => onChange({ ...data, name: e.target.value })}
      />

      <div className="grid grid-cols-2 gap-4">
        <ActionSelector
          action={data.action}
          onChange={(action) => onChange({ ...data, action })}
        />
        <TimeSelector
          hour={data.hour}
          minute={data.minute}
          onChange={(hour, minute) => onChange({ ...data, hour, minute })}
        />
      </div>

      <DaySelector
        days={data.days}
        orderedDays={orderedDays}
        onToggleDay={toggleDay}
        onSetPreset={setPresetDays}
      />

      {data.action === "on" && isDualBoiler && (
        <StrategySelector
          strategy={data.strategy}
          onChange={(strategy) => onChange({ ...data, strategy })}
        />
      )}

      <div className="flex justify-end gap-2 pt-2">
        <Button variant="ghost" onClick={onCancel}>
          <X className="w-4 h-4" />
          Cancel
        </Button>
        <Button onClick={onSave} disabled={data.days === 0} loading={saving}>
          <Save className="w-4 h-4" />
          Save Schedule
        </Button>
      </div>
    </div>
  );
}

// Sub-components

interface ActionSelectorProps {
  action: "on" | "off";
  onChange: (action: "on" | "off") => void;
}

function ActionSelector({ action, onChange }: ActionSelectorProps) {
  return (
    <div>
      <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
        Action
      </label>
      <div className="flex gap-2 h-[50px]">
        <button
          onClick={() => onChange("on")}
          className={`flex-1 px-3 rounded-xl border text-sm font-medium transition-all flex items-center justify-center ${
            action === "on"
              ? "bg-emerald-500/20 border-emerald-500 text-emerald-500"
              : "bg-theme-secondary border-theme text-theme-secondary hover:border-theme-light"
          }`}
        >
          <Power className="w-4 h-4 mr-2" />
          Turn On
        </button>
        <button
          onClick={() => onChange("off")}
          className={`flex-1 px-3 rounded-xl border text-sm font-medium transition-all flex items-center justify-center ${
            action === "off"
              ? "bg-orange-500/20 border-orange-500 text-orange-500"
              : "bg-theme-secondary border-theme text-theme-secondary hover:border-theme-light"
          }`}
        >
          <PowerOff className="w-4 h-4 mr-2" />
          Turn Off
        </button>
      </div>
    </div>
  );
}

interface TimeSelectorProps {
  hour: number;
  minute: number;
  onChange: (hour: number, minute: number) => void;
}

function TimeSelector({ hour, minute, onChange }: TimeSelectorProps) {
  return (
    <div>
      <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
        Time
      </label>
      <div className="flex gap-2 h-[50px]">
        <input
          type="number"
          min={0}
          max={23}
          value={hour}
          onChange={(e) => onChange(parseInt(e.target.value) || 0, minute)}
          className="w-20 text-center px-3 bg-theme-secondary border border-theme rounded-xl text-theme outline-none transition-colors focus:border-accent"
        />
        <span className="self-center text-theme-muted font-bold">:</span>
        <input
          type="number"
          min={0}
          max={59}
          step={5}
          value={minute.toString().padStart(2, "0")}
          onChange={(e) => onChange(hour, parseInt(e.target.value) || 0)}
          className="w-20 text-center px-3 bg-theme-secondary border border-theme rounded-xl text-theme outline-none transition-colors focus:border-accent"
        />
      </div>
    </div>
  );
}

interface DaySelectorProps {
  days: number;
  orderedDays: DayInfo[];
  onToggleDay: (dayValue: number) => void;
  onSetPreset: (preset: "weekdays" | "weekends" | "everyday") => void;
}

function DaySelector({ days, orderedDays, onToggleDay, onSetPreset }: DaySelectorProps) {
  return (
    <div>
      <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
        Days
      </label>
      <div className="flex gap-1.5 mb-2">
        {orderedDays.map((day) => (
          <button
            key={day.value}
            onClick={() => onToggleDay(day.value)}
            className={`w-9 h-9 rounded-lg text-xs font-bold transition-all border ${
              days & day.value
                ? "bg-accent text-white border-accent"
                : "bg-theme-secondary text-theme-muted border-theme hover:border-theme-light"
            }`}
          >
            {day.short}
          </button>
        ))}
      </div>
      <div className="flex gap-2 text-xs">
        <button onClick={() => onSetPreset("weekdays")} className="text-accent hover:underline">
          Mon–Fri
        </button>
        <span className="text-theme-muted">•</span>
        <button onClick={() => onSetPreset("weekends")} className="text-accent hover:underline">
          Sat–Sun
        </button>
        <span className="text-theme-muted">•</span>
        <button onClick={() => onSetPreset("everyday")} className="text-accent hover:underline">
          Every day
        </button>
      </div>
    </div>
  );
}

interface StrategySelectorProps {
  strategy: number;
  onChange: (strategy: number) => void;
}

function StrategySelector({ strategy, onChange }: StrategySelectorProps) {
  return (
    <div>
      <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
        Heating Strategy
      </label>
      <select
        value={strategy}
        onChange={(e) => onChange(parseInt(e.target.value))}
        className="w-full h-[50px] px-4 bg-theme-secondary border border-theme rounded-xl text-theme outline-none transition-colors focus:border-accent"
      >
        {STRATEGIES.map((s) => (
          <option key={s.value} value={s.value}>
            {s.label} - {s.desc}
          </option>
        ))}
      </select>
    </div>
  );
}

