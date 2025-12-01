import React, { useState, useEffect } from 'react';
import { useStore } from '@/lib/store';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import { Clock, Globe, RefreshCw, Save } from 'lucide-react';
import { TIMEZONES } from '../constants/timezones';

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
      }
    } catch (err) {
      console.error('Failed to save time settings:', err);
    }
    setSaving(false);
  };

  const syncNow = async () => {
    setSyncing(true);
    try {
      await fetch('/api/time/sync', { method: 'POST' });
      setTimeout(fetchTimeStatus, 2000); // Fetch status after sync
    } catch (err) {
      console.error('Failed to sync NTP:', err);
    }
    setSyncing(false);
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Clock className="w-5 h-5" />}>Time & Timezone</CardTitle>
      </CardHeader>

      {/* Current Time Status */}
      <div className="mb-6 p-4 bg-cream-100 rounded-xl">
        <div className="flex items-center justify-between mb-2">
          <span className="text-sm text-coffee-500">Current Time</span>
          <Badge variant={timeStatus?.synced ? 'success' : 'warning'}>
            {timeStatus?.synced ? 'Synced' : 'Not synced'}
          </Badge>
        </div>
        <div className="text-2xl font-mono font-bold text-coffee-900">
          {timeStatus?.currentTime || '—'}
        </div>
        <div className="text-sm text-coffee-500 mt-1">
          {timeStatus?.timezone || 'Unknown timezone'}
        </div>
      </div>

      <p className="text-sm text-coffee-500 mb-4">
        Configure time synchronization for accurate schedule triggers.
      </p>

      <div className="space-y-4">
        {/* NTP Server */}
        <Input
          label="NTP Server"
          value={settings.ntpServer}
          onChange={(e) => setSettings({ ...settings, ntpServer: e.target.value })}
          placeholder="pool.ntp.org"
          hint="Network Time Protocol server"
        />

        {/* Timezone */}
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500 mb-1.5">
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

        {/* DST */}
        <div className="flex items-center justify-between p-3 bg-cream-50 rounded-xl">
          <div>
            <div className="font-medium text-coffee-800">Daylight Saving Time</div>
            <div className="text-sm text-coffee-500">
              Add {settings.dstOffsetMinutes} minutes during DST period
            </div>
          </div>
          <Toggle
            checked={settings.dstEnabled}
            onChange={(enabled) => setSettings({ ...settings, dstEnabled: enabled })}
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
      </div>

      <div className="flex gap-3 mt-6">
        <Button variant="secondary" onClick={syncNow} loading={syncing}>
          <RefreshCw className="w-4 h-4" />
          Sync Now
        </Button>
        <Button onClick={saveSettings} loading={saving}>
          <Save className="w-4 h-4" />
          Save Time Settings
        </Button>
      </div>
    </Card>
  );
}

export function RegionalSettings() {
  const preferences = useStore((s) => s.preferences);
  const setPreference = useStore((s) => s.setPreference);

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Globe className="w-5 h-5" />}>Regional</CardTitle>
      </CardHeader>
      <p className="text-sm text-coffee-500 mb-4">
        Customize display settings for your region.
      </p>
      
      <div className="space-y-4">
        <div>
          <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500 mb-1.5">
            First Day of Week
          </label>
          <div className="flex gap-2">
            <button
              onClick={() => setPreference('firstDayOfWeek', 'sunday')}
              className={`flex-1 py-2.5 px-4 rounded-xl border text-sm font-medium transition-all ${
                preferences.firstDayOfWeek === 'sunday'
                  ? 'bg-accent text-white border-accent'
                  : 'bg-cream-100 border-coffee-200 text-coffee-600 hover:border-coffee-300'
              }`}
            >
              Sunday
            </button>
            <button
              onClick={() => setPreference('firstDayOfWeek', 'monday')}
              className={`flex-1 py-2.5 px-4 rounded-xl border text-sm font-medium transition-all ${
                preferences.firstDayOfWeek === 'monday'
                  ? 'bg-accent text-white border-accent'
                  : 'bg-cream-100 border-coffee-200 text-coffee-600 hover:border-coffee-300'
              }`}
            >
              Monday
            </button>
          </div>
          <p className="mt-1.5 text-xs text-coffee-400">
            Affects how weekdays and weekends are displayed in schedules
          </p>
        </div>

        <div>
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={preferences.use24HourTime}
              onChange={(e) => setPreference('use24HourTime', e.target.checked)}
              className="w-4 h-4 rounded border-cream-300 text-accent focus:ring-accent"
            />
            <span className="text-sm text-coffee-700">Use 24-hour time format</span>
          </label>
        </div>
      </div>
    </Card>
  );
}

