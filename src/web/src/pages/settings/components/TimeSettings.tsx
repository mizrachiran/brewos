import { useState, useEffect } from 'react';
import { useStore } from '@/lib/store';
import { useCommand } from '@/lib/useCommand';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import { useToast } from '@/components/Toast';
import { Clock, Globe, RefreshCw, Thermometer, Settings, ChevronRight } from 'lucide-react';
import { TIMEZONES } from '../constants/timezones';
import { getUnitLabel } from '@/lib/temperature';
import type { TemperatureUnit, UserPreferences } from '@/lib/types';

interface TimeStatus {
  synced: boolean;
  currentTime: string;
  timezone: string;
  utcOffset: number;
  settings: {
    useNTP: boolean;
    ntpServer: string;
    utcOffsetMinutes: number;
    dstEnabled: boolean;
    dstOffsetMinutes: number;
  };
}

export function TimeSettings() {
  const { success, error } = useToast();
  const [timeStatus, setTimeStatus] = useState<TimeStatus | null>(null);
  const [settings, setSettings] = useState({
    useNTP: true,
    ntpServer: 'pool.ntp.org',
    utcOffsetMinutes: 0,
    dstEnabled: false,
    dstOffsetMinutes: 60,
  });
  const [syncing, setSyncing] = useState(false);
  const [saving, setSaving] = useState(false);
  const [editing, setEditing] = useState(false);

  useEffect(() => {
    fetchTimeStatus();
    const interval = setInterval(fetchTimeStatus, 10000); // Update every 10s
    return () => clearInterval(interval);
  }, []);

  const fetchTimeStatus = async () => {
    try {
      const res = await fetch('/api/time');
      if (res.ok) {
        const data = await res.json();
        setTimeStatus(data);
        if (data.settings) {
          setSettings(data.settings);
        }
      }
    } catch (err) {
      console.error('Failed to fetch time status:', err);
    }
  };

  const saveSettings = async () => {
    setSaving(true);
    try {
      const res = await fetch('/api/time', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings),
      });
      if (res.ok) {
        await fetchTimeStatus();
        success('Time settings saved successfully');
      } else {
        throw new Error('Failed to save');
      }
    } catch (err) {
      console.error('Failed to save time settings:', err);
      error('Failed to save time settings. Please try again.');
    }
    setSaving(false);
  };

  const syncNow = async () => {
    setSyncing(true);
    try {
      const res = await fetch('/api/time/sync', { method: 'POST' });
      if (res.ok) {
        setTimeout(fetchTimeStatus, 2000); // Fetch status after sync
        success('Time sync initiated');
      } else {
        throw new Error('Sync failed');
      }
    } catch (err) {
      console.error('Failed to sync NTP:', err);
      error('Failed to sync time. Please try again.');
    }
    setSyncing(false);
  };

  // Get timezone label from offset
  const getTimezoneLabel = (offsetMinutes: number) => {
    const tz = TIMEZONES.find(t => t.offset === offsetMinutes);
    return tz?.label || `UTC${offsetMinutes >= 0 ? '+' : ''}${offsetMinutes / 60}`;
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Clock className="w-5 h-5" />}>Time & Timezone</CardTitle>
      </CardHeader>

      {/* Current Time Status */}
      <div className="mb-4 p-4 bg-theme-secondary rounded-xl">
        <div className="flex items-center justify-between mb-2">
          <span className="text-sm text-theme-muted">Current Time</span>
          <Badge variant={timeStatus?.synced ? 'success' : 'warning'}>
            {timeStatus?.synced ? 'Synced' : 'Not synced'}
          </Badge>
        </div>
        <div className="text-2xl font-mono font-bold text-theme">
          {timeStatus?.currentTime || '—'}
        </div>
        <div className="text-sm text-theme-muted mt-1">
          {timeStatus?.timezone || 'Unknown timezone'}
        </div>
      </div>

      {!editing ? (
        /* View mode */
        <div className="space-y-0">
          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">NTP Server</span>
            <span className="text-sm font-medium font-mono text-theme">{settings.ntpServer}</span>
          </div>
          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Timezone</span>
            <span className="text-sm font-medium text-theme">{getTimezoneLabel(settings.utcOffsetMinutes)}</span>
          </div>
          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Daylight Saving</span>
            <span className="text-sm font-medium text-theme">{settings.dstEnabled ? `+${settings.dstOffsetMinutes} min` : 'Off'}</span>
          </div>
          
          {/* Configure button */}
          <button
            onClick={() => setEditing(true)}
            className="w-full flex items-center justify-between py-2.5 border-b border-theme text-left group transition-colors hover:opacity-80"
          >
            <div className="flex items-center gap-3">
              <Settings className="w-4 h-4 text-theme-muted" />
              <span className="text-sm font-medium text-theme">Configure Time</span>
            </div>
            <ChevronRight className="w-4 h-4 text-theme-muted group-hover:text-theme transition-colors" />
          </button>
          
          {/* Sync button */}
          <button
            onClick={syncNow}
            disabled={syncing}
            className="w-full flex items-center justify-between py-2.5 text-left group transition-colors hover:opacity-80 disabled:opacity-50"
          >
            <div className="flex items-center gap-3">
              <RefreshCw className={`w-4 h-4 text-theme-muted ${syncing ? 'animate-spin' : ''}`} />
              <span className="text-sm font-medium text-theme">Sync Now</span>
            </div>
            <ChevronRight className="w-4 h-4 text-theme-muted group-hover:text-theme transition-colors" />
          </button>
        </div>
      ) : (
        /* Edit mode */
        <div className="space-y-4">
          <Input
            label="NTP Server"
            value={settings.ntpServer}
            onChange={(e) => setSettings({ ...settings, ntpServer: e.target.value })}
            placeholder="pool.ntp.org"
            hint="Network Time Protocol server"
          />

          <div>
            <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
              Timezone
            </label>
            <select
              value={settings.utcOffsetMinutes}
              onChange={(e) => setSettings({ ...settings, utcOffsetMinutes: parseInt(e.target.value) })}
              className="input"
            >
              {TIMEZONES.map(tz => (
                <option key={tz.offset} value={tz.offset}>
                  {tz.label}
                </option>
              ))}
            </select>
          </div>

          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Daylight Saving Time</span>
            <Toggle
              checked={settings.dstEnabled}
              onChange={(enabled) => setSettings({ ...settings, dstEnabled: enabled })}
              label={settings.dstEnabled ? 'On' : 'Off'}
            />
          </div>

          {settings.dstEnabled && (
            <Input
              label="DST Offset"
              type="number"
              min={0}
              max={120}
              step={15}
              value={settings.dstOffsetMinutes}
              onChange={(e) => setSettings({ ...settings, dstOffsetMinutes: parseInt(e.target.value) })}
              unit="min"
              hint="Usually 60 minutes"
            />
          )}

          <div className="flex justify-end gap-3">
            <Button variant="ghost" onClick={() => setEditing(false)}>
              Cancel
            </Button>
            <Button 
              onClick={() => {
                saveSettings();
                setEditing(false);
              }} 
              loading={saving}
            >
              Save
            </Button>
          </div>
        </div>
      )}
    </Card>
  );
}

export function RegionalSettings() {
  const preferences = useStore((s) => s.preferences);
  const setPreference = useStore((s) => s.setPreference);
  const { sendCommand } = useCommand();
  
  // Save preference both locally and to ESP32
  const updatePreference = <K extends keyof typeof preferences>(key: K, value: typeof preferences[K]) => {
    setPreference(key, value);
    sendCommand('set_preferences', { [key]: value });
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Globe className="w-5 h-5" />}>Regional</CardTitle>
      </CardHeader>
      <p className="text-sm text-theme-muted mb-4">
        Customize display settings for your region.
      </p>
      
      <div className="space-y-4">
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
            First Day of Week
          </label>
          <div className="flex gap-2">
            <button
              onClick={() => updatePreference('firstDayOfWeek', 'sunday')}
              className={`flex-1 py-2.5 px-4 rounded-xl border text-sm font-medium transition-all ${
                preferences.firstDayOfWeek === 'sunday'
                  ? 'bg-accent text-white border-accent'
                  : 'bg-theme-secondary border-theme text-theme-secondary hover:border-theme-light'
              }`}
            >
              Sunday
            </button>
            <button
              onClick={() => updatePreference('firstDayOfWeek', 'monday')}
              className={`flex-1 py-2.5 px-4 rounded-xl border text-sm font-medium transition-all ${
                preferences.firstDayOfWeek === 'monday'
                  ? 'bg-accent text-white border-accent'
                  : 'bg-theme-secondary border-theme text-theme-secondary hover:border-theme-light'
              }`}
            >
              Monday
            </button>
          </div>
          <p className="mt-1.5 text-xs text-theme-muted">
            Affects how weekdays and weekends are displayed in schedules
          </p>
        </div>

        <div>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={preferences.use24HourTime}
              onChange={(e) => updatePreference('use24HourTime', e.target.checked)}
              className="w-4 h-4 rounded border-theme text-accent focus:ring-accent"
            />
            <span className="text-sm text-theme-secondary">Use 24-hour time format</span>
          </label>
        </div>
      </div>
    </Card>
  );
}

export function UnitsSettings() {
  const preferences = useStore((s) => s.preferences);
  const setPreference = useStore((s) => s.setPreference);
  const { sendCommand } = useCommand();
  
  // Save preference both locally and to ESP32
  const updatePreference = <K extends keyof UserPreferences>(key: K, value: UserPreferences[K]) => {
    setPreference(key, value);
    sendCommand('set_preferences', { [key]: value });
  };
  
  const units: TemperatureUnit[] = ['celsius', 'fahrenheit'];
  
  const currencies = [
    { code: 'USD', symbol: '$', name: 'US Dollar' },
    { code: 'EUR', symbol: '€', name: 'Euro' },
    { code: 'GBP', symbol: '£', name: 'British Pound' },
    { code: 'AUD', symbol: 'A$', name: 'Australian Dollar' },
    { code: 'CAD', symbol: 'C$', name: 'Canadian Dollar' },
    { code: 'JPY', symbol: '¥', name: 'Japanese Yen' },
    { code: 'CHF', symbol: 'Fr', name: 'Swiss Franc' },
    { code: 'ILS', symbol: '₪', name: 'Israeli Shekel' },
  ] as const;

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Thermometer className="w-5 h-5" />}>Units & Pricing</CardTitle>
      </CardHeader>
      <p className="text-sm text-theme-muted mb-4">
        Choose your preferred measurement units and electricity pricing.
      </p>
      
      <div className="space-y-6">
        {/* Temperature Unit */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
            Temperature
          </label>
          <div className="flex gap-2">
            {units.map((unit) => (
              <button
                key={unit}
                onClick={() => updatePreference('temperatureUnit', unit)}
                className={`flex-1 py-2.5 px-4 rounded-xl border text-sm font-medium transition-all ${
                  preferences.temperatureUnit === unit
                    ? 'bg-accent text-white border-accent'
                    : 'bg-theme-secondary border-theme text-theme-secondary hover:border-theme-light'
                }`}
              >
                <span className="block font-semibold">{getUnitLabel(unit)}</span>
                <span className="text-xs opacity-80">
                  {unit === 'celsius' ? '°C' : '°F'}
                </span>
              </button>
            ))}
          </div>
          <p className="mt-1.5 text-xs text-theme-muted">
            {preferences.temperatureUnit === 'celsius' 
              ? 'Using metric system (93°C = brew temperature)'
              : 'Using imperial system (200°F = brew temperature)'}
          </p>
        </div>

        {/* Currency */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
            Currency
          </label>
          <select
            value={preferences.currency}
            onChange={(e) => updatePreference('currency', e.target.value as UserPreferences['currency'])}
            className="input w-full"
          >
            {currencies.map((c) => (
              <option key={c.code} value={c.code}>
                {c.symbol} - {c.name} ({c.code})
              </option>
            ))}
          </select>
          <p className="mt-1.5 text-xs text-theme-muted">
            Used for displaying energy cost estimates
          </p>
        </div>

        {/* Electricity Price */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
            Electricity Price
          </label>
          <div className="flex items-center gap-2">
            <input
              type="number"
              min="0"
              max="10"
              step="0.01"
              value={preferences.electricityPrice}
              onChange={(e) => updatePreference('electricityPrice', parseFloat(e.target.value) || 0)}
              className="input flex-1"
            />
            <span className="text-sm text-theme-muted whitespace-nowrap">
              {currencies.find(c => c.code === preferences.currency)?.symbol ?? '$'}/kWh
            </span>
          </div>
          <p className="mt-1.5 text-xs text-theme-muted">
            Your local electricity rate per kilowatt-hour. Check your utility bill for accurate pricing.
          </p>
        </div>
      </div>
    </Card>
  );
}

