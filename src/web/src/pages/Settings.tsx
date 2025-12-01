import { useState, useEffect } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { useAppStore } from '@/lib/mode';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import { QRCodeDisplay } from '@/components/QRCode';
import { cn } from '@/lib/utils';
import { formatUptime, formatBytes } from '@/lib/utils';
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
  Scale as ScaleIcon,
  Bluetooth,
  Battery,
  Loader2,
  Signal,
  Cloud,
  QrCode,
  Copy,
  ExternalLink,
  Shield,
  Cpu,
  HardDrive,
  Download,
  Terminal,
  Trash2,
  AlertTriangle,
  Github,
  Heart,
  Code,
  Info,
  Server,
} from 'lucide-react';
import { QRCodeSVG } from 'qrcode.react';

type SettingsTab = 'machine' | 'temperature' | 'power' | 'network' | 'scale' | 'cloud' | 'time' | 'system' | 'about';

const getSettingsTabs = (isCloud: boolean): { id: SettingsTab; label: string; icon: React.ComponentType<{ className?: string }> }[] => {
  const tabs: { id: SettingsTab; label: string; icon: React.ComponentType<{ className?: string }> }[] = [
    { id: 'machine', label: 'Machine', icon: Coffee },
    { id: 'temperature', label: 'Temperature', icon: Thermometer },
    { id: 'power', label: 'Power', icon: Zap },
    { id: 'network', label: 'Network', icon: Wifi },
    { id: 'scale', label: 'Scale', icon: ScaleIcon },
  ];
  
  // Only show Cloud tab in local mode
  if (!isCloud) {
    tabs.push({ id: 'cloud', label: 'Cloud', icon: Cloud });
  }
  
  tabs.push(
    { id: 'time', label: 'Time', icon: Clock },
    { id: 'system', label: 'System', icon: Server },
    { id: 'about', label: 'About', icon: Info },
  );
  
  return tabs;
};

export function Settings() {
  const location = useLocation();
  const navigate = useNavigate();
  const device = useStore((s) => s.device);
  const temps = useStore((s) => s.temps);
  const power = useStore((s) => s.power);
  const wifi = useStore((s) => s.wifi);
  const mqtt = useStore((s) => s.mqtt);
  const { mode } = useAppStore();
  
  const isCloud = mode === 'cloud';
  const SETTINGS_TABS = getSettingsTabs(isCloud);

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

      {/* Scale Settings */}
      {activeTab === 'scale' && (
        <ScaleSettingsSection />
      )}

      {/* Cloud Settings (local mode only) */}
      {activeTab === 'cloud' && !isCloud && (
        <CloudSettingsSection />
      )}

      {/* Time Settings */}
      {activeTab === 'time' && (
        <>
          <TimeSettingsSection />
          <Card>
            <CardHeader>
              <CardTitle icon={<Globe className="w-5 h-5" />}>Regional</CardTitle>
            </CardHeader>
            <p className="text-sm text-coffee-500 mb-4">
              Customize display settings for your region.
            </p>
            <RegionalSettings />
          </Card>
        </>
      )}

      {/* System Settings */}
      {activeTab === 'system' && (
        <SystemSettingsSection />
      )}

      {/* About */}
      {activeTab === 'about' && (
        <AboutSection />
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

// ===== SCALE SETTINGS SECTION =====
function ScaleSettingsSection() {
  const scale = useStore((s) => s.scale);
  const scanning = useStore((s) => s.scaleScanning);
  const scanResults = useStore((s) => s.scanResults);
  const setScanning = useStore((s) => s.setScaleScanning);
  const clearResults = useStore((s) => s.clearScanResults);

  const startScan = () => {
    clearResults();
    setScanning(true);
    getConnection()?.sendCommand('scale_scan');
  };

  const stopScan = () => {
    setScanning(false);
    getConnection()?.sendCommand('scale_scan_stop');
  };

  const connectScale = (address: string) => {
    getConnection()?.sendCommand('scale_connect', { address });
    setScanning(false);
  };

  const disconnectScale = () => {
    getConnection()?.sendCommand('scale_disconnect');
  };

  const tareScale = () => {
    getConnection()?.sendCommand('tare');
  };

  return (
    <div className="space-y-6">
      {/* Connected Scale */}
      {scale.connected && (
        <Card className="bg-gradient-to-br from-emerald-50 to-cream-100 border-emerald-200">
          <CardHeader
            action={
              <Badge variant="success">
                <Check className="w-3 h-3" />
                Connected
              </Badge>
            }
          >
            <CardTitle icon={<ScaleIcon className="w-5 h-5 text-emerald-600" />}>
              {scale.name || 'BLE Scale'}
            </CardTitle>
          </CardHeader>

          <div className="grid grid-cols-2 sm:grid-cols-4 gap-4 mb-6">
            <ScaleInfoItem label="Type" value={scale.type || 'Unknown'} />
            <ScaleInfoItem 
              label="Weight" 
              value={`${scale.weight.toFixed(1)} g`} 
              highlight 
            />
            <ScaleInfoItem 
              label="Flow Rate" 
              value={`${scale.flowRate.toFixed(1)} g/s`} 
            />
            <ScaleInfoItem 
              label="Battery" 
              value={scale.battery > 0 ? `${scale.battery}%` : 'N/A'}
              icon={<Battery className="w-4 h-4" />}
            />
          </div>

          <div className="flex items-center gap-2 mb-4">
            <span className="text-sm text-coffee-500">Status:</span>
            <Badge variant={scale.stable ? 'success' : 'warning'}>
              {scale.stable ? 'Stable' : 'Unstable'}
            </Badge>
          </div>

          <div className="flex gap-3">
            <Button variant="secondary" onClick={tareScale}>
              Tare
            </Button>
            <Button variant="ghost" onClick={disconnectScale}>
              <X className="w-4 h-4" />
              Disconnect
            </Button>
          </div>
        </Card>
      )}

      {/* Scan for Scales */}
      {!scale.connected && (
        <Card>
          <div className="text-center py-8">
            <div className="w-20 h-20 bg-cream-200 rounded-full flex items-center justify-center mx-auto mb-4">
              <ScaleIcon className="w-10 h-10 text-coffee-400" />
            </div>
            <h2 className="text-xl font-bold text-coffee-900 mb-2">No Scale Connected</h2>
            <p className="text-coffee-500 mb-6">
              Search for nearby Bluetooth scales to pair with your BrewOS device.
            </p>
            
            {scanning ? (
              <Button variant="secondary" onClick={stopScan}>
                <X className="w-4 h-4" />
                Stop Scanning
              </Button>
            ) : (
              <Button onClick={startScan}>
                <Bluetooth className="w-4 h-4" />
                Scan for Scales
              </Button>
            )}
          </div>
        </Card>
      )}

      {/* Scan Results */}
      {(scanning || scanResults.length > 0) && (
        <Card>
          <CardHeader>
            <CardTitle icon={<Bluetooth className="w-5 h-5" />}>
              {scanning ? 'Scanning...' : 'Available Scales'}
            </CardTitle>
            {scanning && <Loader2 className="w-5 h-5 animate-spin text-accent" />}
          </CardHeader>

          {scanResults.length > 0 ? (
            <div className="space-y-2">
              {scanResults.map((result) => (
                <div
                  key={result.address}
                  className="flex items-center justify-between p-4 bg-cream-100 rounded-xl"
                >
                  <div className="flex items-center gap-3">
                    <div className="w-10 h-10 bg-cream-200 rounded-full flex items-center justify-center">
                      <ScaleIcon className="w-5 h-5 text-coffee-500" />
                    </div>
                    <div>
                      <div className="font-semibold text-coffee-900">
                        {result.name || 'Unknown Scale'}
                      </div>
                      <div className="text-xs text-coffee-400 flex items-center gap-2">
                        <Signal className="w-3 h-3" />
                        {result.rssi} dBm
                        {result.type && (
                          <Badge variant="info" className="ml-2">{result.type}</Badge>
                        )}
                      </div>
                    </div>
                  </div>
                  <Button size="sm" onClick={() => connectScale(result.address)}>
                    Connect
                  </Button>
                </div>
              ))}
            </div>
          ) : scanning ? (
            <div className="flex items-center justify-center gap-3 py-8 text-coffee-500">
              <Loader2 className="w-5 h-5 animate-spin" />
              <span>Searching for scales...</span>
            </div>
          ) : (
            <p className="text-center text-coffee-500 py-8">
              No scales found. Make sure your scale is powered on and in pairing mode.
            </p>
          )}
        </Card>
      )}

      {/* Supported Scales */}
      <Card>
        <CardHeader>
          <CardTitle>Supported Scales</CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          {[
            { brand: 'Acaia', models: 'Lunar, Pearl, Pyxis' },
            { brand: 'Decent', models: 'DE1 Scale' },
            { brand: 'Felicita', models: 'Arc, Incline, Parallel' },
            { brand: 'Timemore', models: 'Black Mirror 2' },
            { brand: 'Bookoo', models: 'Themis' },
            { brand: 'Generic', models: 'BT Weight Scale Service' },
          ].map((item) => (
            <div
              key={item.brand}
              className="p-4 bg-cream-100 rounded-xl"
            >
              <div className="font-semibold text-coffee-800">{item.brand}</div>
              <div className="text-sm text-coffee-500">{item.models}</div>
            </div>
          ))}
        </div>
      </Card>
    </div>
  );
}

interface ScaleInfoItemProps {
  label: string;
  value: string;
  highlight?: boolean;
  icon?: React.ReactNode;
}

function ScaleInfoItem({ label, value, highlight, icon }: ScaleInfoItemProps) {
  return (
    <div className="p-3 bg-white/60 rounded-xl">
      <div className="text-xs text-coffee-500 mb-1">{label}</div>
      <div className={`flex items-center gap-1.5 ${highlight ? 'text-xl font-bold' : 'font-semibold'} text-coffee-900`}>
        {icon}
        {value}
      </div>
    </div>
  );
}

// ===== CLOUD SETTINGS SECTION =====
interface PairingData {
  deviceId: string;
  token: string;
  url: string;
  expiresIn: number;
}

interface CloudStatus {
  enabled: boolean;
  connected: boolean;
  serverUrl: string;
}

function CloudSettingsSection() {
  const [cloudConfig, setCloudConfig] = useState<CloudStatus | null>(null);
  const [pairing, setPairing] = useState<PairingData | null>(null);
  const [loadingQR, setLoadingQR] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);
  const [copiedCode, setCopiedCode] = useState(false);
  
  const [cloudEnabled, setCloudEnabled] = useState(cloudConfig?.enabled || false);
  const [cloudUrl, setCloudUrl] = useState(cloudConfig?.serverUrl || 'wss://cloud.brewos.io');
  const [saving, setSaving] = useState(false);

  const fetchPairingQR = async () => {
    setLoadingQR(true);
    setError(null);
    try {
      const response = await fetch('/api/pairing/qr');
      if (!response.ok) throw new Error('Failed to generate pairing code');
      const data = await response.json();
      setPairing(data);
    } catch {
      setError('Failed to generate pairing code. Make sure you\'re connected to your machine.');
    } finally {
      setLoadingQR(false);
    }
  };

  const refreshPairing = async () => {
    setLoadingQR(true);
    setError(null);
    try {
      const response = await fetch('/api/pairing/refresh', { method: 'POST' });
      if (!response.ok) throw new Error('Failed to refresh pairing code');
      const data = await response.json();
      setPairing(data);
    } catch {
      setError('Failed to refresh pairing code.');
    } finally {
      setLoadingQR(false);
    }
  };

  const copyPairingUrl = () => {
    if (pairing?.url) {
      navigator.clipboard.writeText(pairing.url);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const copyPairingCode = () => {
    if (pairing) {
      const code = `${pairing.deviceId}:${pairing.token}`;
      navigator.clipboard.writeText(code);
      setCopiedCode(true);
      setTimeout(() => setCopiedCode(false), 2000);
    }
  };

  const saveSettings = async () => {
    setSaving(true);
    try {
      getConnection()?.sendCommand('set_cloud_config', {
        enabled: cloudEnabled,
        serverUrl: cloudUrl,
      });
      setCloudConfig({
        enabled: cloudEnabled,
        connected: cloudConfig?.connected || false,
        serverUrl: cloudUrl,
      });
    } finally {
      setSaving(false);
    }
  };

  const fetchCloudStatus = async () => {
    try {
      const response = await fetch('/api/cloud/status');
      if (response.ok) {
        const data = await response.json();
        setCloudConfig(data);
        setCloudEnabled(data.enabled);
        setCloudUrl(data.serverUrl || 'wss://cloud.brewos.io');
      }
    } catch {
      // Device might not support cloud status endpoint yet
    }
  };

  useEffect(() => {
    fetchPairingQR();
    fetchCloudStatus();
  }, []);

  const isExpired = pairing?.expiresIn !== undefined && pairing.expiresIn <= 0;

  return (
    <div className="space-y-6">
      <div className="grid gap-6 md:grid-cols-2">
        {/* Pairing QR Code */}
        <Card>
          <div className="flex items-center gap-3 mb-4">
            <div className="p-2 bg-accent/10 rounded-lg">
              <QrCode className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-semibold text-coffee-900">Pair with Cloud</h2>
              <p className="text-sm text-coffee-500">Scan to add to your account</p>
            </div>
          </div>

          <div className="bg-white rounded-xl p-6 border border-cream-200 flex flex-col items-center">
            {loadingQR ? (
              <div className="w-48 h-48 flex items-center justify-center">
                <Loader2 className="w-8 h-8 animate-spin text-accent" />
              </div>
            ) : error ? (
              <div className="w-48 h-48 flex flex-col items-center justify-center text-center">
                <X className="w-8 h-8 text-red-500 mb-2" />
                <p className="text-sm text-coffee-500">{error}</p>
                <Button variant="secondary" size="sm" onClick={fetchPairingQR} className="mt-3">
                  Retry
                </Button>
              </div>
            ) : pairing ? (
              <>
                <div className={`p-3 bg-white rounded-xl ${isExpired ? 'opacity-50' : ''}`}>
                  <QRCodeSVG value={pairing.url} size={180} level="M" includeMargin={false} />
                </div>
                {isExpired ? (
                  <Badge variant="warning" className="mt-3">Code expired</Badge>
                ) : (
                  <p className="text-xs text-coffee-400 mt-2">
                    Expires in {Math.floor(pairing.expiresIn / 60)}m {pairing.expiresIn % 60}s
                  </p>
                )}
              </>
            ) : null}
          </div>

          <div className="mt-4 space-y-3">
            <div className="flex gap-2">
              <Button variant="secondary" className="flex-1" onClick={refreshPairing} disabled={loadingQR}>
                <RefreshCw className={`w-4 h-4 mr-2 ${loadingQR ? 'animate-spin' : ''}`} />
                New Code
              </Button>
              <Button variant="secondary" onClick={copyPairingUrl} disabled={!pairing}>
                {copied ? <Check className="w-4 h-4" /> : <Copy className="w-4 h-4" />}
              </Button>
            </div>
            
            {pairing && (
              <a 
                href={pairing.url}
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center justify-center gap-2 text-sm text-accent hover:underline"
              >
                Open pairing link
                <ExternalLink className="w-3 h-3" />
              </a>
            )}
          </div>

          {pairing && (
            <div className="mt-4 pt-4 border-t border-cream-200">
              <h3 className="text-sm font-medium text-coffee-900 mb-2">Manual Pairing Code</h3>
              <div className="flex items-center gap-2">
                <code className="flex-1 bg-cream-100 px-3 py-2 rounded-lg text-xs font-mono text-coffee-700 break-all">
                  {pairing.deviceId}:{pairing.token}
                </code>
                <Button variant="secondary" size="sm" onClick={copyPairingCode}>
                  {copiedCode ? <Check className="w-4 h-4" /> : <Copy className="w-4 h-4" />}
                </Button>
              </div>
              <p className="text-xs text-coffee-400 mt-2">
                Enter this code at cloud.brewos.io if you can't scan the QR
              </p>
            </div>
          )}
        </Card>

        {/* Cloud Status & Settings */}
        <div className="space-y-6">
          <Card>
            <div className="flex items-center gap-3 mb-4">
              <div className="p-2 bg-accent/10 rounded-lg">
                <Cloud className="w-5 h-5 text-accent" />
              </div>
              <div>
                <h2 className="font-semibold text-coffee-900">Cloud Status</h2>
                <p className="text-sm text-coffee-500">Connection to BrewOS Cloud</p>
              </div>
            </div>

            <div className="space-y-3">
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Wifi className="w-4 h-4 text-coffee-400" />
                  <span className="text-sm text-coffee-700">Cloud Connection</span>
                </div>
                <Badge variant={cloudConfig?.connected ? 'success' : 'default'}>
                  {cloudConfig?.connected ? 'Connected' : 'Disconnected'}
                </Badge>
              </div>
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Globe className="w-4 h-4 text-coffee-400" />
                  <span className="text-sm text-coffee-700">Server</span>
                </div>
                <span className="text-sm text-coffee-500 font-mono">
                  {cloudConfig?.serverUrl || 'Not configured'}
                </span>
              </div>
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Shield className="w-4 h-4 text-coffee-400" />
                  <span className="text-sm text-coffee-700">Device ID</span>
                </div>
                <span className="text-sm text-coffee-500 font-mono">
                  {pairing?.deviceId || '—'}
                </span>
              </div>
            </div>
          </Card>

          <Card>
            <div className="flex items-center gap-3 mb-4">
              <div className="p-2 bg-coffee-100 rounded-lg">
                <Cloud className="w-5 h-5 text-coffee-600" />
              </div>
              <div>
                <h2 className="font-semibold text-coffee-900">Cloud Settings</h2>
                <p className="text-sm text-coffee-500">Configure cloud connection</p>
              </div>
            </div>

            <div className="space-y-4">
              <div>
                <Toggle label="Enable Cloud Connection" checked={cloudEnabled} onChange={setCloudEnabled} />
                <p className="text-xs text-coffee-400 mt-1 ml-14">Allow remote access via BrewOS Cloud</p>
              </div>
              <Input
                label="Cloud Server URL"
                value={cloudUrl}
                onChange={(e) => setCloudUrl(e.target.value)}
                placeholder="wss://cloud.brewos.io"
                disabled={!cloudEnabled}
              />
              <Button onClick={saveSettings} loading={saving} disabled={!cloudEnabled && !cloudConfig?.enabled} className="w-full">
                Save Settings
              </Button>
            </div>
          </Card>
        </div>
      </div>
    </div>
  );
}

// ===== SYSTEM SETTINGS SECTION =====
function SystemSettingsSection() {
  const esp32 = useStore((s) => s.esp32);
  const pico = useStore((s) => s.pico);
  const logs = useStore((s) => s.logs);
  const clearLogs = useStore((s) => s.clearLogs);

  const [checkingUpdate, setCheckingUpdate] = useState(false);
  const [updateAvailable, setUpdateAvailable] = useState<string | null>(null);

  const checkForUpdates = async () => {
    setCheckingUpdate(true);
    getConnection()?.sendCommand('check_update');
    setTimeout(() => {
      setCheckingUpdate(false);
      setUpdateAvailable(null);
    }, 2000);
  };

  const startOTA = () => {
    if (confirm('Start firmware update? The device will restart after update.')) {
      getConnection()?.sendCommand('ota_start');
    }
  };

  const restartDevice = () => {
    if (confirm('Restart the device?')) {
      getConnection()?.sendCommand('restart');
    }
  };

  const factoryReset = () => {
    if (confirm('This will erase all settings. Are you sure?')) {
      if (confirm('Really? This cannot be undone!')) {
        getConnection()?.sendCommand('factory_reset');
      }
    }
  };

  return (
    <div className="space-y-6">
      {/* Device Info */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <Card>
          <CardHeader>
            <CardTitle icon={<Cpu className="w-5 h-5" />}>ESP32-S3</CardTitle>
            <Badge variant="success">Online</Badge>
          </CardHeader>
          <div className="space-y-3">
            <StatusRow label="Firmware" value={esp32.version || 'Unknown'} mono />
            <StatusRow label="Uptime" value={formatUptime(esp32.uptime)} />
            <StatusRow label="Free Heap" value={formatBytes(esp32.freeHeap)} />
          </div>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle icon={<HardDrive className="w-5 h-5" />}>Raspberry Pi Pico</CardTitle>
            <Badge variant={pico.connected ? 'success' : 'error'}>
              {pico.connected ? 'Connected' : 'Disconnected'}
            </Badge>
          </CardHeader>
          <div className="space-y-3">
            <StatusRow label="Firmware" value={pico.version || 'Unknown'} mono />
            <StatusRow label="Uptime" value={formatUptime(pico.uptime)} />
            <StatusRow label="Status" value={pico.connected ? 'Communicating' : 'No response'} />
          </div>
        </Card>
      </div>

      {/* Firmware Update */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Download className="w-5 h-5" />}>Firmware Update</CardTitle>
        </CardHeader>

        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4">
          <div>
            <p className="text-sm text-coffee-700 mb-1">
              Current version: <span className="font-mono font-semibold">{esp32.version || 'Unknown'}</span>
            </p>
            {updateAvailable ? (
              <p className="text-sm text-emerald-600">
                <Check className="w-4 h-4 inline mr-1" />
                Update available: {updateAvailable}
              </p>
            ) : (
              <p className="text-sm text-coffee-500">Check for the latest firmware updates.</p>
            )}
          </div>
          <div className="flex gap-3">
            <Button variant="secondary" onClick={checkForUpdates} loading={checkingUpdate}>
              <RefreshCw className="w-4 h-4" />
              Check for Updates
            </Button>
            {updateAvailable && (
              <Button onClick={startOTA}>
                <Download className="w-4 h-4" />
                Install Update
              </Button>
            )}
          </div>
        </div>
      </Card>

      {/* Logs */}
      <Card>
        <CardHeader
          action={
            <Button variant="ghost" size="sm" onClick={clearLogs}>
              <Trash2 className="w-4 h-4" />
              Clear
            </Button>
          }
        >
          <CardTitle icon={<Terminal className="w-5 h-5" />}>System Logs</CardTitle>
        </CardHeader>

        <div className="max-h-64 overflow-y-auto bg-coffee-900 rounded-xl p-4 font-mono text-xs">
          {logs.length > 0 ? (
            logs.map((log) => (
              <div key={log.id} className="py-1 border-b border-coffee-800 last:border-0">
                <span className="text-coffee-500">{new Date(log.time).toLocaleTimeString()}</span>
                <span className={`ml-2 ${getLogColor(log.level)}`}>[{log.level.toUpperCase()}]</span>
                <span className="text-cream-200 ml-2">{log.message}</span>
              </div>
            ))
          ) : (
            <p className="text-coffee-500 text-center py-4">No logs yet</p>
          )}
        </div>
      </Card>

      {/* Danger Zone */}
      <Card className="border-red-200">
        <CardHeader>
          <CardTitle icon={<AlertTriangle className="w-5 h-5 text-red-500" />}>
            <span className="text-red-600">Danger Zone</span>
          </CardTitle>
        </CardHeader>

        <div className="flex flex-col sm:flex-row gap-4">
          <div className="flex-1">
            <h4 className="font-semibold text-coffee-800 mb-1">Restart Device</h4>
            <p className="text-sm text-coffee-500">Reboot the ESP32. Settings will be preserved.</p>
          </div>
          <Button variant="secondary" onClick={restartDevice}>
            <RefreshCw className="w-4 h-4" />
            Restart
          </Button>
        </div>

        <hr className="my-4 border-cream-200" />

        <div className="flex flex-col sm:flex-row gap-4">
          <div className="flex-1">
            <h4 className="font-semibold text-red-600 mb-1">Factory Reset</h4>
            <p className="text-sm text-coffee-500">Erase all settings and return to factory defaults. This cannot be undone.</p>
          </div>
          <Button variant="danger" onClick={factoryReset}>
            <Trash2 className="w-4 h-4" />
            Factory Reset
          </Button>
        </div>
      </Card>
    </div>
  );
}

function getLogColor(level: string): string {
  switch (level.toLowerCase()) {
    case 'error': return 'text-red-400';
    case 'warn':
    case 'warning': return 'text-amber-400';
    case 'info': return 'text-blue-400';
    case 'debug': return 'text-gray-400';
    default: return 'text-cream-400';
  }
}

// ===== ABOUT SECTION =====
function AboutSection() {
  const esp32 = useStore((s) => s.esp32);
  const pico = useStore((s) => s.pico);

  return (
    <div className="space-y-6">
      {/* Hero */}
      <Card className="text-center bg-gradient-to-br from-coffee-800 to-coffee-900 text-white border-0">
        <div className="py-8">
          <img src="/logo.png" alt="BrewOS" className="h-20 mx-auto mb-6 brightness-0 invert" />
          <p className="text-cream-300 mb-4">Open-source espresso machine controller</p>
          <div className="flex items-center justify-center gap-4 text-sm text-cream-400">
            <span>ESP32: {esp32.version || 'Unknown'}</span>
            <span>•</span>
            <span>Pico: {pico.version || 'Unknown'}</span>
          </div>
        </div>
      </Card>

      {/* Features */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <AboutFeatureCard
          icon={<Cpu className="w-6 h-6" />}
          title="Dual-Core Control"
          description="ESP32-S3 for connectivity, Pico for real-time control"
        />
        <AboutFeatureCard
          icon={<Code className="w-6 h-6" />}
          title="Fully Open Source"
          description="Hardware and software designs available on GitHub"
        />
        <AboutFeatureCard
          icon={<Heart className="w-6 h-6" />}
          title="Community Driven"
          description="Built by and for the espresso community"
        />
      </div>

      {/* Links */}
      <Card>
        <CardHeader>
          <CardTitle>Resources</CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
          <AboutLinkCard
            icon={<Github className="w-5 h-5" />}
            title="GitHub Repository"
            description="Source code and documentation"
            href="https://github.com/mizrachiran/brewos"
          />
          <AboutLinkCard
            icon={<ExternalLink className="w-5 h-5" />}
            title="Documentation"
            description="Setup guides and API reference"
            href="https://brewos.coffee"
          />
        </div>
      </Card>

      {/* System Info */}
      <Card>
        <CardHeader>
          <CardTitle>System Information</CardTitle>
        </CardHeader>

        <div className="grid grid-cols-2 sm:grid-cols-4 gap-4 text-sm">
          <div>
            <span className="text-coffee-500">ESP32 Version</span>
            <p className="font-mono font-medium text-coffee-900">{esp32.version || '—'}</p>
          </div>
          <div>
            <span className="text-coffee-500">Pico Version</span>
            <p className="font-mono font-medium text-coffee-900">{pico.version || '—'}</p>
          </div>
          <div>
            <span className="text-coffee-500">UI Version</span>
            <p className="font-mono font-medium text-coffee-900">1.0.0</p>
          </div>
          <div>
            <span className="text-coffee-500">Build</span>
            <p className="font-mono font-medium text-coffee-900">
              {import.meta.env.MODE === 'production' ? 'Production' : 'Development'}
            </p>
          </div>
        </div>
      </Card>

      {/* Credits */}
      <Card className="text-center">
        <p className="text-coffee-500 mb-2">
          Made with <Heart className="w-4 h-4 inline text-red-500" /> for espresso lovers
        </p>
        <p className="text-sm text-coffee-400">© {new Date().getFullYear()} BrewOS Contributors</p>
      </Card>
    </div>
  );
}

interface AboutFeatureCardProps {
  icon: React.ReactNode;
  title: string;
  description: string;
}

function AboutFeatureCard({ icon, title, description }: AboutFeatureCardProps) {
  return (
    <Card className="text-center">
      <div className="w-12 h-12 bg-accent/10 rounded-xl flex items-center justify-center mx-auto mb-3 text-accent">
        {icon}
      </div>
      <h3 className="font-semibold text-coffee-900 mb-1">{title}</h3>
      <p className="text-sm text-coffee-500">{description}</p>
    </Card>
  );
}

interface AboutLinkCardProps {
  icon: React.ReactNode;
  title: string;
  description: string;
  href: string;
}

function AboutLinkCard({ icon, title, description, href }: AboutLinkCardProps) {
  return (
    <a
      href={href}
      target="_blank"
      rel="noopener noreferrer"
      className="flex items-start gap-4 p-4 bg-cream-100 rounded-xl hover:bg-cream-200 transition-colors group"
    >
      <div className="w-10 h-10 bg-white rounded-lg flex items-center justify-center text-coffee-600 group-hover:text-accent transition-colors">
        {icon}
      </div>
      <div>
        <h4 className="font-semibold text-coffee-900 group-hover:text-accent transition-colors">{title}</h4>
        <p className="text-sm text-coffee-500">{description}</p>
      </div>
    </a>
  );
}

