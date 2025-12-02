import { Button } from "@/components/Button";
import { Toggle } from "@/components/Toggle";
import { Power, PowerOff, Trash2, Flame } from "lucide-react";
import { type Schedule, type DayInfo, STRATEGIES, WEEKDAYS, WEEKENDS, EVERY_DAY } from "./types";

interface ScheduleItemProps {
  schedule: Schedule;
  orderedDays: DayInfo[];
  isDualBoiler: boolean;
  use24HourTime: boolean;
  onEdit: () => void;
  onDelete: () => void;
  onToggle: (enabled: boolean) => void;
}

export function ScheduleItem({
  schedule,
  orderedDays,
  isDualBoiler,
  use24HourTime,
  onEdit,
  onDelete,
  onToggle,
}: ScheduleItemProps) {
  const formatTime = (hour: number, minute: number) => {
    const m = minute.toString().padStart(2, "0");
    if (use24HourTime) {
      return `${hour.toString().padStart(2, "0")}:${m}`;
    }
    const h = hour % 12 || 12;
    const ampm = hour >= 12 ? "PM" : "AM";
    return `${h}:${m} ${ampm}`;
  };

  const getDaysLabel = (days: number) => {
    if (days === EVERY_DAY) return "Every day";
    if (days === WEEKDAYS) return "Weekdays";
    if (days === WEEKENDS) return "Weekends";
    const dayNames = orderedDays.filter((d) => days & d.value).map((d) => d.label);
    return dayNames.join(", ");
  };

  return (
    <div className="flex items-center justify-between">
      <div className="flex items-center gap-4">
        <div
          className={`p-2 rounded-lg ${
            schedule.action === "on"
              ? "bg-emerald-500/20 text-emerald-500"
              : "bg-orange-500/20 text-orange-500"
          }`}
        >
          {schedule.action === "on" ? (
            <Power className="w-5 h-5" />
          ) : (
            <PowerOff className="w-5 h-5" />
          )}
        </div>
        <div>
          <div className="font-medium text-theme">
            {schedule.name || `${schedule.action === "on" ? "Turn On" : "Turn Off"}`}
          </div>
          <div className="text-sm text-theme-muted">
            {formatTime(schedule.hour, schedule.minute)} • {getDaysLabel(schedule.days)}
          </div>
          {schedule.action === "on" && isDualBoiler && (
            <div className="text-xs text-theme-muted mt-1">
              <Flame className="w-3 h-3 inline mr-1" />
              {STRATEGIES.find((s) => s.value === schedule.strategy)?.label || "Sequential"}
            </div>
          )}
        </div>
      </div>
      <div className="flex items-center gap-2">
        <Toggle checked={schedule.enabled} onChange={onToggle} />
        <Button variant="ghost" size="sm" onClick={onEdit}>
          Edit
        </Button>
        <Button variant="ghost" size="sm" onClick={onDelete}>
          <Trash2 className="w-4 h-4 text-red-500" />
        </Button>
      </div>
    </div>
  );
}

