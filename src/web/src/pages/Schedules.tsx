import { useState, useEffect, useMemo } from 'react';
import { useStore } from '@/lib/store';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Button } from '@/components/Button';
import { Input } from '@/components/Input';
import { Toggle } from '@/components/Toggle';
import { 
  Clock, 
  Plus, 
  Trash2, 
  Power, 
  PowerOff,
  Moon,
  Calendar,
  Flame,
  Save,
  X,
} from 'lucide-react';

// Days of week - starting from Sunday (standard ISO)
const ALL_DAYS = [
  { value: 0x01, label: 'Sun', short: 'S', index: 0 },
  { value: 0x02, label: 'Mon', short: 'M', index: 1 },
  { value: 0x04, label: 'Tue', short: 'T', index: 2 },
  { value: 0x08, label: 'Wed', short: 'W', index: 3 },
  { value: 0x10, label: 'Thu', short: 'T', index: 4 },
  { value: 0x20, label: 'Fri', short: 'F', index: 5 },
  { value: 0x40, label: 'Sat', short: 'S', index: 6 },
];

const WEEKDAYS = 0x3E; // Mon-Fri
const WEEKENDS = 0x41; // Sat-Sun
const EVERY_DAY = 0x7F;

// Helper to reorder days based on first day of week
const getOrderedDays = (firstDay: 'sunday' | 'monday') => {
  if (firstDay === 'monday') {
    // Mon, Tue, Wed, Thu, Fri, Sat, Sun
    return [...ALL_DAYS.slice(1), ALL_DAYS[0]];
  }
  return ALL_DAYS;
};

// Heating strategies
const STRATEGIES = [
  { value: 0, label: 'Brew Only', desc: 'Heat only brew boiler' },
  { value: 1, label: 'Sequential', desc: 'Brew first, then steam' },
  { value: 2, label: 'Parallel', desc: 'Heat both simultaneously' },
  { value: 3, label: 'Steam Priority', desc: 'Steam first, then brew' },
  { value: 4, label: 'Smart Stagger', desc: 'Power-aware heating' },
];

interface Schedule {
  id: number;
  enabled: boolean;
  days: number;
  hour: number;
  minute: number;
  action: 'on' | 'off';
  strategy: number;
  name: string;
}

interface ScheduleFormData {
  enabled: boolean;
  days: number;
  hour: number;
  minute: number;
  action: 'on' | 'off';
  strategy: number;
  name: string;
}

const defaultSchedule: ScheduleFormData = {
  enabled: true,
  days: WEEKDAYS,
  hour: 7,
  minute: 0,
  action: 'on',
  strategy: 1,
  name: '',
};

export function Schedules() {
  const preferences = useStore((s) => s.preferences);
  const [schedules, setSchedules] = useState<Schedule[]>([]);
  const [autoPowerOff, setAutoPowerOff] = useState({ enabled: false, minutes: 60 });
  const [isEditing, setIsEditing] = useState<number | null>(null);
  const [isAdding, setIsAdding] = useState(false);
  const [formData, setFormData] = useState<ScheduleFormData>(defaultSchedule);
  const [loading, setLoading] = useState(true);

  // Get ordered days based on user preference
  const orderedDays = useMemo(() => 
    getOrderedDays(preferences.firstDayOfWeek), 
    [preferences.firstDayOfWeek]
  );

  // Fetch schedules on mount
  useEffect(() => {
    fetchSchedules();
  }, []);

  const fetchSchedules = async () => {
    try {
      const res = await fetch('/api/schedules');
      const data = await res.json();
      setSchedules(data.schedules || []);
      setAutoPowerOff({
        enabled: data.autoPowerOffEnabled || false,
        minutes: data.autoPowerOffMinutes || 60,
      });
    } catch (err) {
      console.error('Failed to fetch schedules:', err);
    } finally {
      setLoading(false);
    }
  };

  const saveSchedule = async () => {
    try {
      if (isEditing) {
        await fetch('/api/schedules/update', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id: isEditing, ...formData }),
        });
      } else {
        await fetch('/api/schedules', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(formData),
        });
      }
      await fetchSchedules();
      setIsEditing(null);
      setIsAdding(false);
      setFormData(defaultSchedule);
    } catch (err) {
      console.error('Failed to save schedule:', err);
    }
  };

  const deleteSchedule = async (id: number) => {
    if (!confirm('Delete this schedule?')) return;
    try {
      await fetch('/api/schedules/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id }),
      });
      await fetchSchedules();
    } catch (err) {
      console.error('Failed to delete schedule:', err);
    }
  };

  const toggleSchedule = async (id: number, enabled: boolean) => {
    try {
      await fetch('/api/schedules/toggle', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id, enabled }),
      });
      await fetchSchedules();
    } catch (err) {
      console.error('Failed to toggle schedule:', err);
    }
  };

  const saveAutoPowerOff = async () => {
    try {
      await fetch('/api/schedules/auto-off', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(autoPowerOff),
      });
    } catch (err) {
      console.error('Failed to save auto power-off:', err);
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
    setFormData(defaultSchedule);
  };

  const cancelEdit = () => {
    setIsEditing(null);
    setIsAdding(false);
    setFormData(defaultSchedule);
  };

  const toggleDay = (dayValue: number) => {
    setFormData(prev => ({
      ...prev,
      days: prev.days ^ dayValue,
    }));
  };

  const setPresetDays = (preset: 'weekdays' | 'weekends' | 'everyday') => {
    const days = preset === 'weekdays' ? WEEKDAYS : 
                 preset === 'weekends' ? WEEKENDS : EVERY_DAY;
    setFormData(prev => ({ ...prev, days }));
  };

  const formatTime = (hour: number, minute: number) => {
    const m = minute.toString().padStart(2, '0');
    if (preferences.use24HourTime) {
      return `${hour.toString().padStart(2, '0')}:${m}`;
    }
    const h = hour % 12 || 12;
    const ampm = hour >= 12 ? 'PM' : 'AM';
    return `${h}:${m} ${ampm}`;
  };

  const getDaysLabel = (days: number) => {
    if (days === EVERY_DAY) return 'Every day';
    if (days === WEEKDAYS) return 'Weekdays';
    if (days === WEEKENDS) return 'Weekends';
    
    const dayNames = orderedDays.filter(d => days & d.value).map(d => d.label);
    return dayNames.join(', ');
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-coffee-600"></div>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Schedule List */}
      <Card>
        <CardHeader
          action={
            <Button onClick={startAdding} disabled={isAdding || schedules.length >= 10}>
              <Plus className="w-4 h-4" />
              Add Schedule
            </Button>
          }
        >
          <CardTitle icon={<Calendar className="w-5 h-5" />}>
            Schedules
          </CardTitle>
        </CardHeader>

        <p className="text-sm text-coffee-500 mb-4">
          Schedule your machine to turn on or off at specific times.
        </p>

        {schedules.length === 0 && !isAdding ? (
          <div className="text-center py-8 text-coffee-400">
            <Clock className="w-12 h-12 mx-auto mb-3 opacity-50" />
            <p>No schedules configured</p>
            <p className="text-sm">Add a schedule to automate your machine</p>
          </div>
        ) : (
          <div className="space-y-3">
            {schedules.map(schedule => (
              <div
                key={schedule.id}
                className={`p-4 rounded-xl border transition-all ${
                  schedule.enabled 
                    ? 'bg-cream-50 border-cream-200' 
                    : 'bg-cream-100/50 border-cream-200/50 opacity-60'
                } ${isEditing === schedule.id ? 'ring-2 ring-accent' : ''}`}
              >
                {isEditing === schedule.id ? (
                  <ScheduleForm
                    data={formData}
                    onChange={setFormData}
                    onSave={saveSchedule}
                    onCancel={cancelEdit}
                    toggleDay={toggleDay}
                    setPresetDays={setPresetDays}
                    orderedDays={orderedDays}
                  />
                ) : (
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-4">
                      <div className={`p-2 rounded-lg ${
                        schedule.action === 'on' 
                          ? 'bg-emerald-100 text-emerald-600' 
                          : 'bg-orange-100 text-orange-600'
                      }`}>
                        {schedule.action === 'on' ? <Power className="w-5 h-5" /> : <PowerOff className="w-5 h-5" />}
                      </div>
                      <div>
                        <div className="font-medium text-coffee-900">
                          {schedule.name || `${schedule.action === 'on' ? 'Turn On' : 'Turn Off'}`}
                        </div>
                        <div className="text-sm text-coffee-500">
                          {formatTime(schedule.hour, schedule.minute)} • {getDaysLabel(schedule.days)}
                        </div>
                        {schedule.action === 'on' && (
                          <div className="text-xs text-coffee-400 mt-1">
                            <Flame className="w-3 h-3 inline mr-1" />
                            {STRATEGIES.find(s => s.value === schedule.strategy)?.label || 'Sequential'}
                          </div>
                        )}
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      <Toggle
                        checked={schedule.enabled}
                        onChange={(enabled) => toggleSchedule(schedule.id, enabled)}
                      />
                      <Button variant="ghost" size="sm" onClick={() => editSchedule(schedule)}>
                        Edit
                      </Button>
                      <Button variant="ghost" size="sm" onClick={() => deleteSchedule(schedule.id)}>
                        <Trash2 className="w-4 h-4 text-red-500" />
                      </Button>
                    </div>
                  </div>
                )}
              </div>
            ))}

            {isAdding && (
              <div className="p-4 rounded-xl border border-accent bg-accent/5">
                <ScheduleForm
                  data={formData}
                  onChange={setFormData}
                  onSave={saveSchedule}
                  onCancel={cancelEdit}
                  toggleDay={toggleDay}
                  setPresetDays={setPresetDays}
                  orderedDays={orderedDays}
                />
              </div>
            )}
          </div>
        )}
      </Card>

      {/* Auto Power-Off */}
      <Card>
        <CardHeader
          action={
            <Toggle
              checked={autoPowerOff.enabled}
              onChange={(enabled) => setAutoPowerOff({ ...autoPowerOff, enabled })}
            />
          }
        >
          <CardTitle icon={<Moon className="w-5 h-5" />}>
            Auto Power-Off
          </CardTitle>
        </CardHeader>

        <p className="text-sm text-coffee-500 mb-4">
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
            onChange={(e) => setAutoPowerOff({ ...autoPowerOff, minutes: parseInt(e.target.value) })}
            unit="minutes"
            disabled={!autoPowerOff.enabled}
          />
        </div>

        <Button onClick={saveAutoPowerOff}>
          <Save className="w-4 h-4" />
          Save Auto Power-Off
        </Button>
      </Card>
    </div>
  );
}

// Schedule Form Component
interface DayInfo {
  value: number;
  label: string;
  short: string;
  index: number;
}

interface ScheduleFormProps {
  data: ScheduleFormData;
  onChange: (data: ScheduleFormData) => void;
  onSave: () => void;
  onCancel: () => void;
  toggleDay: (day: number) => void;
  setPresetDays: (preset: 'weekdays' | 'weekends' | 'everyday') => void;
  orderedDays: DayInfo[];
}

function ScheduleForm({ data, onChange, onSave, onCancel, toggleDay, setPresetDays, orderedDays }: ScheduleFormProps) {
  return (
    <div className="space-y-4">
      <Input
        label="Name (optional)"
        placeholder="Morning brew, Evening shutdown..."
        value={data.name}
        onChange={(e) => onChange({ ...data, name: e.target.value })}
      />

      <div className="grid grid-cols-2 gap-4">
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500 mb-1.5">
            Action
          </label>
          <div className="flex gap-2 h-[50px]">
            <button
              onClick={() => onChange({ ...data, action: 'on' })}
              className={`flex-1 px-3 rounded-xl border-2 text-sm font-medium transition-all flex items-center justify-center ${
                data.action === 'on'
                  ? 'bg-emerald-100 border-emerald-400 text-emerald-700'
                  : 'bg-cream-100 border-cream-300 text-coffee-600 hover:border-coffee-300'
              }`}
            >
              <Power className="w-4 h-4 mr-2" />
              Turn On
            </button>
            <button
              onClick={() => onChange({ ...data, action: 'off' })}
              className={`flex-1 px-3 rounded-xl border-2 text-sm font-medium transition-all flex items-center justify-center ${
                data.action === 'off'
                  ? 'bg-orange-100 border-orange-400 text-orange-700'
                  : 'bg-cream-100 border-cream-300 text-coffee-600 hover:border-coffee-300'
              }`}
            >
              <PowerOff className="w-4 h-4 mr-2" />
              Turn Off
            </button>
          </div>
        </div>

        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500 mb-1.5">
            Time
          </label>
          <div className="flex gap-2 h-[50px]">
            <input
              type="number"
              min={0}
              max={23}
              value={data.hour}
              onChange={(e) => onChange({ ...data, hour: parseInt(e.target.value) || 0 })}
              className="w-20 text-center px-3 bg-cream-100 border-2 border-cream-300 rounded-xl text-coffee-900 outline-none transition-colors focus:border-accent focus:bg-white"
            />
            <span className="self-center text-coffee-400 font-bold">:</span>
            <input
              type="number"
              min={0}
              max={59}
              step={5}
              value={data.minute.toString().padStart(2, '0')}
              onChange={(e) => onChange({ ...data, minute: parseInt(e.target.value) || 0 })}
              className="w-20 text-center px-3 bg-cream-100 border-2 border-cream-300 rounded-xl text-coffee-900 outline-none transition-colors focus:border-accent focus:bg-white"
            />
          </div>
        </div>
      </div>

      <div>
        <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500 mb-1.5">
          Days
        </label>
        <div className="flex gap-1.5 mb-2">
          {orderedDays.map(day => (
            <button
              key={day.value}
              onClick={() => toggleDay(day.value)}
              className={`w-9 h-9 rounded-lg text-xs font-bold transition-all border-2 ${
                data.days & day.value
                  ? 'bg-accent text-white border-accent'
                  : 'bg-cream-100 text-coffee-500 border-cream-300 hover:border-coffee-300'
              }`}
            >
              {day.short}
            </button>
          ))}
        </div>
        <div className="flex gap-2 text-xs">
          <button
            onClick={() => setPresetDays('weekdays')}
            className="text-accent hover:underline"
          >
            Mon–Fri
          </button>
          <span className="text-coffee-300">•</span>
          <button
            onClick={() => setPresetDays('weekends')}
            className="text-accent hover:underline"
          >
            Sat–Sun
          </button>
          <span className="text-coffee-300">•</span>
          <button
            onClick={() => setPresetDays('everyday')}
            className="text-accent hover:underline"
          >
            Every day
          </button>
        </div>
      </div>

      {data.action === 'on' && (
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500 mb-1.5">
            Heating Strategy
          </label>
          <select
            value={data.strategy}
            onChange={(e) => onChange({ ...data, strategy: parseInt(e.target.value) })}
            className="w-full h-[50px] px-4 bg-cream-100 border-2 border-cream-300 rounded-xl text-coffee-900 outline-none transition-colors focus:border-accent focus:bg-white"
          >
            {STRATEGIES.map(s => (
              <option key={s.value} value={s.value}>
                {s.label} - {s.desc}
              </option>
            ))}
          </select>
        </div>
      )}

      <div className="flex justify-end gap-2 pt-2">
        <Button variant="ghost" onClick={onCancel}>
          <X className="w-4 h-4" />
          Cancel
        </Button>
        <Button onClick={onSave} disabled={data.days === 0}>
          <Save className="w-4 h-4" />
          Save Schedule
        </Button>
      </div>
    </div>
  );
}

