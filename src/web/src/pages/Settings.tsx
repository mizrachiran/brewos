import { useState, useEffect } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import { cn } from '@/lib/utils';
import { 
  Thermometer, 
  Zap, 
  Wifi, 
  Radio, 
  Leaf,
  Check,
  X,
  Coffee,
  Save,
  Globe,
  Clock,
  RefreshCw,
} from 'lucide-react';

type SettingsTab = 'machine' | 'temperature' | 'power' | 'network' | 'time' | 'regional';

const SETTINGS_TABS: { id: SettingsTab; label: string; icon: React.ComponentType<{ className?: string }> }[] = [
  { id: 'machine', label: 'Machine', icon: Coffee },
  { id: 'temperature', label: 'Temperature', icon: Thermometer },
  { id: 'power', label: 'Power & Energy', icon: Zap },
  { id: 'network', label: 'Network', icon: Wifi },
  { id: 'time', label: 'Time', icon: Clock },
  { id: 'regional', label: 'Regional', icon: Globe },
];

export function Settings() {
  const location = useLocation();
  const navigate = useNavigate();
  const device = useStore((s) => s.device);
  const temps = useStore((s) => s.temps);
  const power = useStore((s) => s.power);
  const wifi = useStore((s) => s.wifi);
  const mqtt = useStore((s) => s.mqtt);

  // Get active tab from URL hash, default to 'machine'
  const hashTab = location.hash.slice(1) as SettingsTab;
  const activeTab = SETTINGS_TABS.some(t => t.id === hashTab) ? hashTab : 'machine';
  
  const setActiveTab = (tab: SettingsTab) => {
    navigate({ hash: tab }, { replace: true });
  };

  // Device identity
  const [deviceName, setDeviceName] = useState(device.deviceName);
  const [machineBrand, setMachineBrand] = useState(device.machineBrand || '');
  const [machineModel, setMachineModel] = useState(device.machineModel || '');
  const [savingMachine, setSavingMachine] = useState(false);
  
  // Sync device info when it changes from server
  useEffect(() => {
    setDeviceName(device.deviceName);
    setMachineBrand(device.machineBrand || '');
    setMachineModel(device.machineModel || '');
  }, [device.deviceName, device.machineBrand, device.machineModel]);

  // Local state for forms
  const [brewTemp, setBrewTemp] = useState(temps.brew.setpoint);
  const [steamTemp, setSteamTemp] = useState(temps.steam.setpoint);
  const [voltage, setVoltage] = useState(power.voltage);
  const [maxCurrent, setMaxCurrent] = useState(13);
  const [mqttConfig, setMqttConfig] = useState({
    enabled: mqtt.enabled,
    broker: mqtt.broker || '',
    port: 1883,
    username: '',
    password: '',
    topic: mqtt.topic || 'brewos',
    discovery: true,
  });
  const [ecoSettings, setEcoSettings] = useState({
    brewTemp: 80,
    timeout: 30,
  });
  const [testingMqtt, setTestingMqtt] = useState(false);

  const saveMachineInfo = async () => {
    if (!deviceName.trim() || !machineBrand.trim() || !machineModel.trim()) return;
    setSavingMachine(true);
    getConnection()?.sendCommand('set_machine_info', { 
      name: deviceName.trim(),
      brand: machineBrand.trim(),
      model: machineModel.trim(),
    });
    // Simulate success
    setTimeout(() => setSavingMachine(false), 500);
  };
  
  const isMachineInfoValid = deviceName.trim() && machineBrand.trim() && machineModel.trim();
  const isMachineInfoChanged = 
    deviceName !== device.deviceName || 
    machineBrand !== (device.machineBrand || '') || 
    machineModel !== (device.machineModel || '');

  const saveTemps = () => {
    getConnection()?.sendCommand('set_temp', { boiler: 'brew', temp: brewTemp });
    getConnection()?.sendCommand('set_temp', { boiler: 'steam', temp: steamTemp });
  };

  const savePower = () => {
    getConnection()?.sendCommand('set_power', { voltage, maxCurrent });
  };

  const testMqtt = async () => {
    setTestingMqtt(true);
    getConnection()?.sendCommand('mqtt_test', mqttConfig);
    setTimeout(() => setTestingMqtt(false), 3000);
  };

  const saveMqtt = () => {
    getConnection()?.sendCommand('mqtt_config', mqttConfig);
  };

  const saveEco = () => {
    getConnection()?.sendCommand('set_eco', ecoSettings);
  };

  const forgetWifi = () => {
    if (confirm('Are you sure? The device will restart in AP mode.')) {
      getConnection()?.sendCommand('wifi_forget');
    }
  };

  return (
    <div className="space-y-6">
      {/* Tab Navigation */}
      <div className="bg-white rounded-xl border border-coffee-200 p-2">
        <div className="flex gap-1 overflow-x-auto scrollbar-hide">
          {SETTINGS_TABS.map((tab) => {
            const Icon = tab.icon;
            const isActive = activeTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={cn(
                  'flex items-center gap-2 px-3 sm:px-4 py-2 rounded-lg text-sm font-medium whitespace-nowrap transition-all',
                  isActive
                    ? 'bg-coffee-800 text-white shadow-soft'
                    : 'text-coffee-600 hover:bg-cream-200'
                )}
                title={tab.label}
              >
                <Icon className="w-4 h-4 flex-shrink-0" />
                <span className="hidden sm:inline">{tab.label}</span>
              </button>
            );
          })}
        </div>
      </div>

      {/* Device Identity */}
      {activeTab === 'machine' && (
        <Card>
        <CardHeader>
          <CardTitle icon={<Coffee className="w-5 h-5" />}>
            Machine
          </CardTitle>
        </CardHeader>

        <div className="space-y-4">
          <Input
            label="Machine Name"
            placeholder="Kitchen Espresso"
            value={deviceName}
            onChange={(e) => setDeviceName(e.target.value)}
            hint="Give your machine a friendly name"
            required
          />
          
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <Input
              label="Brand"
              placeholder="ECM, Profitec, Rancilio..."
              value={machineBrand}
              onChange={(e) => setMachineBrand(e.target.value)}
              hint="Machine manufacturer"
              required
            />
            <Input
              label="Model"
              placeholder="Synchronika, Pro 700, Silvia..."
              value={machineModel}
              onChange={(e) => setMachineModel(e.target.value)}
              hint="Machine model name"
              required
            />
          </div>
          
          <div className="flex justify-end">
            <Button 
              onClick={saveMachineInfo} 
              loading={savingMachine}
              disabled={!isMachineInfoValid || !isMachineInfoChanged}
            >
              <Save className="w-4 h-4" />
              Save Machine Info
            </Button>
          </div>
          
          {device.deviceId && (
            <div className="grid grid-cols-2 gap-4 pt-2 border-t border-cream-200">
              <StatusRow label="Device ID" value={device.deviceId} mono />
              <StatusRow label="Machine Type" value={device.machineType || 'Not set'} />
            </div>
          )}
        </div>
      </Card>
      )}

      {/* Temperature Settings */}
      {activeTab === 'temperature' && (
        <Card>
        <CardHeader>
          <CardTitle icon={<Thermometer className="w-5 h-5" />}>
            Temperature
          </CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
          <Input
            label="Brew Temperature"
            type="number"
            min={80}
            max={105}
            step={0.5}
            value={brewTemp}
            onChange={(e) => setBrewTemp(parseFloat(e.target.value))}
            unit="°C"
            hint="Recommended: 92-96°C"
          />
          <Input
            label="Steam Temperature"
            type="number"
            min={120}
            max={160}
            step={1}
            value={steamTemp}
            onChange={(e) => setSteamTemp(parseFloat(e.target.value))}
            unit="°C"
            hint="For milk frothing"
          />
        </div>

        <Button onClick={saveTemps}>Save Temperatures</Button>
      </Card>
      )}

      {/* Power & Energy Settings */}
      {activeTab === 'power' && (
        <>
          {/* Power Settings */}
          <Card>
            <CardHeader>
              <CardTitle icon={<Zap className="w-5 h-5" />}>Power</CardTitle>
            </CardHeader>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
              <div className="space-y-1.5">
                <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500">
                  Mains Voltage
                </label>
                <select
                  value={voltage}
                  onChange={(e) => setVoltage(parseInt(e.target.value))}
                  className="input"
                >
                  <option value="110">110V (US)</option>
                  <option value="220">220V (EU/AU)</option>
                  <option value="240">240V (UK)</option>
                </select>
              </div>
              <Input
                label="Max Current"
                type="number"
                min={5}
                max={20}
                step={0.5}
                value={maxCurrent}
                onChange={(e) => setMaxCurrent(parseFloat(e.target.value))}
                unit="A"
                hint="Limit for your circuit"
              />
            </div>

            <Button onClick={savePower}>Save Power Settings</Button>
          </Card>

          {/* Eco Mode */}
          <Card>
            <CardHeader>
              <CardTitle icon={<Leaf className="w-5 h-5" />}>Eco Mode</CardTitle>
            </CardHeader>

            <p className="text-sm text-coffee-500 mb-4">
              Reduce power consumption when idle by lowering boiler temperatures.
            </p>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
              <Input
                label="Eco Brew Temp"
                type="number"
                min={60}
                max={90}
                value={ecoSettings.brewTemp}
                onChange={(e) => setEcoSettings({ ...ecoSettings, brewTemp: parseFloat(e.target.value) })}
                unit="°C"
              />
              <Input
                label="Auto-Eco After"
                type="number"
                min={5}
                max={120}
                step={5}
                value={ecoSettings.timeout}
                onChange={(e) => setEcoSettings({ ...ecoSettings, timeout: parseInt(e.target.value) })}
                unit="min"
              />
            </div>

            <Button onClick={saveEco}>Save Eco Settings</Button>
          </Card>
        </>
      )}

      {/* Network Settings (WiFi + MQTT) */}
      {activeTab === 'network' && (
        <>
          {/* WiFi Status */}
          <Card>
        <CardHeader>
          <CardTitle icon={<Wifi className="w-5 h-5" />}>WiFi</CardTitle>
        </CardHeader>

        <div className="space-y-3 mb-6">
          <StatusRow 
            label="Status" 
            value={
              <Badge variant={wifi.connected ? 'success' : wifi.apMode ? 'warning' : 'error'}>
                {wifi.connected ? 'Connected' : wifi.apMode ? 'AP Mode' : 'Disconnected'}
              </Badge>
            }
          />
          <StatusRow label="Network" value={wifi.ssid || '—'} />
          <StatusRow label="IP Address" value={wifi.ip || '—'} mono />
          <StatusRow 
            label="Signal" 
            value={wifi.rssi ? `${wifi.rssi} dBm` : '—'} 
          />
        </div>

        <Button variant="ghost" onClick={forgetWifi}>
          <X className="w-4 h-4" />
          Forget Network
        </Button>
      </Card>

      {/* MQTT Settings */}
      <Card>
        <CardHeader
          action={
            <Toggle
              checked={mqttConfig.enabled}
              onChange={(enabled) => setMqttConfig({ ...mqttConfig, enabled })}
            />
          }
        >
          <CardTitle icon={<Radio className="w-5 h-5" />}>
            MQTT / Home Assistant
          </CardTitle>
        </CardHeader>

        <div className="space-y-4 mb-6">
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <Input
              label="Broker Address"
              placeholder="homeassistant.local"
              value={mqttConfig.broker}
              onChange={(e) => setMqttConfig({ ...mqttConfig, broker: e.target.value })}
            />
            <Input
              label="Topic Prefix"
              placeholder="brewos"
              value={mqttConfig.topic}
              onChange={(e) => setMqttConfig({ ...mqttConfig, topic: e.target.value })}
              hint="Topics: {prefix}/status, {prefix}/command"
            />
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
            <Input
              label="Port"
              type="number"
              value={mqttConfig.port}
              onChange={(e) => setMqttConfig({ ...mqttConfig, port: parseInt(e.target.value) })}
            />
            <Input
              label="Username"
              placeholder="Optional"
              value={mqttConfig.username}
              onChange={(e) => setMqttConfig({ ...mqttConfig, username: e.target.value })}
            />
            <Input
              label="Password"
              type="password"
              placeholder="Optional"
              value={mqttConfig.password}
              onChange={(e) => setMqttConfig({ ...mqttConfig, password: e.target.value })}
            />
          </div>

          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={mqttConfig.discovery}
              onChange={(e) => setMqttConfig({ ...mqttConfig, discovery: e.target.checked })}
              className="w-4 h-4 rounded border-cream-300 text-accent focus:ring-accent"
            />
            <span className="text-sm text-coffee-700">Enable Home Assistant auto-discovery</span>
          </label>

          <div className="flex items-center gap-2 p-3 bg-cream-100 rounded-xl">
            {mqtt.connected ? (
              <Check className="w-4 h-4 text-emerald-600" />
            ) : mqtt.enabled ? (
              <X className="w-4 h-4 text-red-500" />
            ) : (
              <div className="w-4 h-4 rounded-full bg-cream-300" />
            )}
            <span className="text-sm text-coffee-700">
              {mqtt.connected ? 'Connected to broker' : mqtt.enabled ? 'Disconnected' : 'Disabled'}
            </span>
          </div>
        </div>

        <div className="flex gap-3">
          <Button variant="secondary" onClick={testMqtt} loading={testingMqtt}>
            {testingMqtt ? 'Testing...' : 'Test Connection'}
          </Button>
          <Button onClick={saveMqtt}>Save MQTT Settings</Button>
        </div>
      </Card>
        </>
      )}

      {/* Regional Settings */}
      {activeTab === 'regional' && (
        <Card>
        <CardHeader>
          <CardTitle icon={<Globe className="w-5 h-5" />}>Regional</CardTitle>
        </CardHeader>

        <p className="text-sm text-coffee-500 mb-4">
          Customize display settings for your region.
        </p>

        <RegionalSettings />
      </Card>
      )}

      {/* Time Settings */}
      {activeTab === 'time' && (
        <TimeSettingsSection />
      )}
    </div>
  );
}

interface StatusRowProps {
  label: string;
  value: React.ReactNode;
  mono?: boolean;
}

function StatusRow({ label, value, mono }: StatusRowProps) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-cream-200 last:border-0">
      <span className="text-sm text-coffee-500">{label}</span>
      <span className={`text-sm font-medium text-coffee-900 ${mono ? 'font-mono' : ''}`}>
        {value}
      </span>
    </div>
  );
}

function RegionalSettings() {
  const preferences = useStore((s) => s.preferences);
  const setPreference = useStore((s) => s.setPreference);

  return (
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
  );
}

// Common timezone offsets
const TIMEZONES = [
  { offset: -720, label: 'UTC-12:00 (Baker Island)' },
  { offset: -660, label: 'UTC-11:00 (Samoa)' },
  { offset: -600, label: 'UTC-10:00 (Hawaii)' },
  { offset: -540, label: 'UTC-09:00 (Alaska)' },
  { offset: -480, label: 'UTC-08:00 (Pacific Time)' },
  { offset: -420, label: 'UTC-07:00 (Mountain Time)' },
  { offset: -360, label: 'UTC-06:00 (Central Time)' },
  { offset: -300, label: 'UTC-05:00 (Eastern Time)' },
  { offset: -240, label: 'UTC-04:00 (Atlantic Time)' },
  { offset: -180, label: 'UTC-03:00 (Buenos Aires)' },
  { offset: -120, label: 'UTC-02:00 (Mid-Atlantic)' },
  { offset: -60, label: 'UTC-01:00 (Azores)' },
  { offset: 0, label: 'UTC+00:00 (London, Dublin)' },
  { offset: 60, label: 'UTC+01:00 (Paris, Berlin)' },
  { offset: 120, label: 'UTC+02:00 (Jerusalem, Athens)' },
  { offset: 180, label: 'UTC+03:00 (Moscow, Istanbul)' },
  { offset: 210, label: 'UTC+03:30 (Tehran)' },
  { offset: 240, label: 'UTC+04:00 (Dubai)' },
  { offset: 270, label: 'UTC+04:30 (Kabul)' },
  { offset: 300, label: 'UTC+05:00 (Karachi)' },
  { offset: 330, label: 'UTC+05:30 (Mumbai, Delhi)' },
  { offset: 345, label: 'UTC+05:45 (Kathmandu)' },
  { offset: 360, label: 'UTC+06:00 (Dhaka)' },
  { offset: 420, label: 'UTC+07:00 (Bangkok)' },
  { offset: 480, label: 'UTC+08:00 (Singapore, Hong Kong)' },
  { offset: 540, label: 'UTC+09:00 (Tokyo, Seoul)' },
  { offset: 570, label: 'UTC+09:30 (Adelaide)' },
  { offset: 600, label: 'UTC+10:00 (Sydney)' },
  { offset: 660, label: 'UTC+11:00 (Solomon Islands)' },
  { offset: 720, label: 'UTC+12:00 (Auckland)' },
];

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

function TimeSettingsSection() {
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

