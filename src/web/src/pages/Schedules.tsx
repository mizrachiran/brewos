import { useState, useEffect, useMemo, useCallback } from "react";
import { useStore } from "@/lib/store";
import { useMobileLandscape } from "@/lib/useMobileLandscape";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Toggle } from "@/components/Toggle";
import { PageHeader } from "@/components/PageHeader";
import { useToast } from "@/components/Toast";
import { useConfirmDialog } from "@/components/ConfirmDialogProvider";
import {
  ScheduleForm,
  ScheduleItem,
  type Schedule,
  type ScheduleFormData,
  DEFAULT_SCHEDULE,
  getOrderedDays,
} from "@/components/schedules";
import { isDemoMode, getDemoSchedules } from "@/lib/demo-mode";
import { Clock, Plus, Moon, Calendar, Save } from "lucide-react";

export function Schedules() {
  const preferences = useStore((s) => s.preferences);
  const device = useStore((s) => s.device);
  const { success, error } = useToast();
  const { confirm } = useConfirmDialog();

  const [schedules, setSchedules] = useState<Schedule[]>([]);
  const [autoPowerOff, setAutoPowerOff] = useState({
    enabled: false,
    minutes: 60,
  });
  const [isEditing, setIsEditing] = useState<number | null>(null);
  const [isAdding, setIsAdding] = useState(false);
  const [formData, setFormData] = useState<ScheduleFormData>(DEFAULT_SCHEDULE);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);

  const isDualBoiler = device.machineType === "dual_boiler";
  const orderedDays = useMemo(
    () => getOrderedDays(preferences.firstDayOfWeek),
    [preferences.firstDayOfWeek]
  );

  const isDemo = isDemoMode();

  const fetchSchedules = useCallback(async () => {
    // Use mock data in demo mode
    if (isDemo) {
      const demoData = getDemoSchedules();
      setSchedules(demoData.schedules as Schedule[]);
      setAutoPowerOff({
        enabled: demoData.autoPowerOffEnabled,
        minutes: demoData.autoPowerOffMinutes,
      });
      setLoading(false);
      return;
    }

    try {
      const res = await fetch("/api/schedules");
      const data = await res.json();
      setSchedules(data.schedules || []);
      setAutoPowerOff({
        enabled: data.autoPowerOffEnabled || false,
        minutes: data.autoPowerOffMinutes || 60,
      });
    } catch (err) {
      console.error("Failed to fetch schedules:", err);
    } finally {
      setLoading(false);
    }
  }, [isDemo]);

  useEffect(() => {
    fetchSchedules();
  }, [fetchSchedules]);

  const saveSchedule = async () => {
    setSaving(true);

    // Demo mode: simulate save locally
    if (isDemo) {
      await new Promise((r) => setTimeout(r, 300)); // Brief delay for UX
      if (isEditing) {
        setSchedules((prev) =>
          prev.map((s) => (s.id === isEditing ? { ...s, ...formData } : s))
        );
      } else {
        const newSchedule: Schedule = {
          id: Date.now(),
          ...formData,
        };
        setSchedules((prev) => [...prev, newSchedule]);
      }
      resetForm();
      success(isEditing ? "Schedule updated" : "Schedule created");
      setSaving(false);
      return;
    }

    try {
      const endpoint = isEditing ? "/api/schedules/update" : "/api/schedules";
      const body = isEditing ? { id: isEditing, ...formData } : formData;

      const res = await fetch(endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
      });

      if (!res.ok) throw new Error("Failed to save");

      await fetchSchedules();
      resetForm();
      success(isEditing ? "Schedule updated" : "Schedule created");
    } catch (err) {
      console.error("Failed to save schedule:", err);
      error("Failed to save schedule. Please try again.");
    } finally {
      setSaving(false);
    }
  };

  const deleteSchedule = async (id: number) => {
    const confirmed = await confirm({
      title: "Delete Schedule?",
      description: "This schedule will be permanently removed.",
      variant: "danger",
      confirmText: "Delete",
    });
    if (!confirmed) return;

    // Demo mode: delete locally
    if (isDemo) {
      setSchedules((prev) => prev.filter((s) => s.id !== id));
      success("Schedule deleted");
      return;
    }

    try {
      const res = await fetch("/api/schedules/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id }),
      });
      if (!res.ok) throw new Error("Failed to delete");
      await fetchSchedules();
      success("Schedule deleted");
    } catch (err) {
      console.error("Failed to delete schedule:", err);
      error("Failed to delete schedule. Please try again.");
    }
  };

  const toggleSchedule = async (id: number, enabled: boolean) => {
    // Demo mode: toggle locally
    if (isDemo) {
      setSchedules((prev) =>
        prev.map((s) => (s.id === id ? { ...s, enabled } : s))
      );
      success(`Schedule ${enabled ? "enabled" : "disabled"}`);
      return;
    }

    try {
      const res = await fetch("/api/schedules/toggle", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id, enabled }),
      });
      if (!res.ok) throw new Error("Failed to toggle");
      await fetchSchedules();
      success(`Schedule ${enabled ? "enabled" : "disabled"}`);
    } catch (err) {
      console.error("Failed to toggle schedule:", err);
      error("Failed to update schedule. Please try again.");
    }
  };

  const saveAutoPowerOff = async () => {
    // Demo mode: just show success
    if (isDemo) {
      success("Auto power-off settings saved");
      return;
    }

    try {
      const res = await fetch("/api/schedules/auto-off", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(autoPowerOff),
      });
      if (!res.ok) throw new Error("Failed to save");
      success("Auto power-off settings saved");
    } catch (err) {
      console.error("Failed to save auto power-off:", err);
      error("Failed to save auto power-off settings.");
    }
  };

  const editSchedule = (schedule: Schedule) => {
    setIsEditing(schedule.id);
    setFormData({
      enabled: schedule.enabled,
      days: schedule.days,
      hour: schedule.hour,
      minute: schedule.minute,
      action: schedule.action,
      strategy: schedule.strategy,
      name: schedule.name,
    });
    setIsAdding(false);
  };

  const startAdding = () => {
    setIsAdding(true);
    setIsEditing(null);
    setFormData(DEFAULT_SCHEDULE);
  };

  const resetForm = () => {
    setIsEditing(null);
    setIsAdding(false);
    setFormData(DEFAULT_SCHEDULE);
  };

  const isMobileLandscape = useMobileLandscape();
  const sectionGap = isMobileLandscape ? "space-y-3" : "space-y-6";

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-accent" />
      </div>
    );
  }

  return (
    <div className={sectionGap}>
      <PageHeader
        title="Schedules"
        subtitle="Automate your machine power cycles"
      />

      {/* Schedule List */}
      <Card>
        <CardHeader
          action={
            <Button
              onClick={startAdding}
              disabled={isAdding || schedules.length >= 10}
            >
              <Plus className="w-4 h-4" />
              Add Schedule
            </Button>
          }
        >
          <CardTitle icon={<Calendar className="w-5 h-5" />}>
            Schedules
          </CardTitle>
        </CardHeader>

        <p className="text-sm text-theme-muted mb-4">
          Schedule your machine to turn on or off at specific times.
        </p>

        {schedules.length === 0 && !isAdding ? (
          <EmptySchedules />
        ) : (
          <div className="space-y-3">
            {schedules.map((schedule) => (
              <ScheduleCard
                key={schedule.id}
                schedule={schedule}
                isEditing={isEditing === schedule.id}
                formData={formData}
                orderedDays={orderedDays}
                isDualBoiler={isDualBoiler}
                use24HourTime={preferences.use24HourTime}
                saving={saving}
                onFormChange={setFormData}
                onSave={saveSchedule}
                onCancel={resetForm}
                onEdit={() => editSchedule(schedule)}
                onDelete={() => deleteSchedule(schedule.id)}
                onToggle={(enabled) => toggleSchedule(schedule.id, enabled)}
              />
            ))}

            {isAdding && (
              <div className="p-4 rounded-xl border border-accent bg-accent/5">
                <ScheduleForm
                  data={formData}
                  onChange={setFormData}
                  onSave={saveSchedule}
                  onCancel={resetForm}
                  orderedDays={orderedDays}
                  isDualBoiler={isDualBoiler}
                  saving={saving}
                />
              </div>
            )}
          </div>
        )}
      </Card>

      {/* Auto Power-Off */}
      <AutoPowerOffCard
        autoPowerOff={autoPowerOff}
        onChange={setAutoPowerOff}
        onSave={saveAutoPowerOff}
      />
    </div>
  );
}

// Sub-components

function EmptySchedules() {
  return (
    <div className="text-center py-8 text-theme-muted">
      <Clock className="w-12 h-12 mx-auto mb-3 opacity-50" />
      <p>No schedules configured</p>
      <p className="text-sm">Add a schedule to automate your machine</p>
    </div>
  );
}

interface ScheduleCardProps {
  schedule: Schedule;
  isEditing: boolean;
  formData: ScheduleFormData;
  orderedDays: ReturnType<typeof getOrderedDays>;
  isDualBoiler: boolean;
  use24HourTime: boolean;
  saving: boolean;
  onFormChange: (data: ScheduleFormData) => void;
  onSave: () => void;
  onCancel: () => void;
  onEdit: () => void;
  onDelete: () => void;
  onToggle: (enabled: boolean) => void;
}

function ScheduleCard({
  schedule,
  isEditing,
  formData,
  orderedDays,
  isDualBoiler,
  use24HourTime,
  saving,
  onFormChange,
  onSave,
  onCancel,
  onEdit,
  onDelete,
  onToggle,
}: ScheduleCardProps) {
  return (
    <div
      className={`p-4 rounded-xl border transition-all ${
        schedule.enabled
          ? "bg-theme-secondary border-theme"
          : "bg-theme-secondary/50 border-theme/50 opacity-60"
      } ${isEditing ? "ring-2 ring-accent" : ""}`}
    >
      {isEditing ? (
        <ScheduleForm
          data={formData}
          onChange={onFormChange}
          onSave={onSave}
          onCancel={onCancel}
          orderedDays={orderedDays}
          isDualBoiler={isDualBoiler}
          saving={saving}
        />
      ) : (
        <ScheduleItem
          schedule={schedule}
          orderedDays={orderedDays}
          isDualBoiler={isDualBoiler}
          use24HourTime={use24HourTime}
          onEdit={onEdit}
          onDelete={onDelete}
          onToggle={onToggle}
        />
      )}
    </div>
  );
}

interface AutoPowerOffCardProps {
  autoPowerOff: { enabled: boolean; minutes: number };
  onChange: (value: { enabled: boolean; minutes: number }) => void;
  onSave: () => void;
}

function AutoPowerOffCard({
  autoPowerOff,
  onChange,
  onSave,
}: AutoPowerOffCardProps) {
  return (
    <Card>
      <CardHeader
        action={
          <Toggle
            checked={autoPowerOff.enabled}
            onChange={(enabled) => onChange({ ...autoPowerOff, enabled })}
          />
        }
      >
        <CardTitle icon={<Moon className="w-5 h-5" />}>
          Auto Power-Off
        </CardTitle>
      </CardHeader>

      <p className="text-sm text-theme-muted mb-4">
        Automatically turn off the machine after a period of inactivity.
      </p>

      <div className="flex items-center gap-4 mb-4">
        <Input
          label="Idle timeout"
          type="number"
          min={5}
          max={480}
          step={5}
          value={autoPowerOff.minutes}
          onChange={(e) =>
            onChange({ ...autoPowerOff, minutes: parseInt(e.target.value) })
          }
          unit="minutes"
          disabled={!autoPowerOff.enabled}
        />
      </div>

      <Button onClick={onSave}>
        <Save className="w-4 h-4" />
        Save Auto Power-Off
      </Button>
    </Card>
  );
}
