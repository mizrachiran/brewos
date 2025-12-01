import { useState, useEffect } from "react";
import { useStore } from "@/lib/store";
import { getConnection } from "@/lib/connection";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Toggle } from "@/components/Toggle";
import { Badge } from "@/components/Badge";
import { QRCodeDisplay } from "@/components/QRCode";
import { LogViewer } from "@/components/LogViewer";
import {
  Cpu,
  HardDrive,
  Download,
  RefreshCw,
  Terminal,
  Trash2,
  AlertTriangle,
  Check,
  Clock,
  Cloud,
  Globe,
  Save,
} from "lucide-react";
import { formatUptime, formatBytes } from "@/lib/utils";
import { formatDate } from "@/lib/date";
import { isGoogleAuthConfigured } from "@/lib/google-auth";

interface PairingData {
  deviceId: string;
  token: string;
  url: string;
  expiresIn: number;
}

export function System() {
  const esp32 = useStore((s) => s.esp32);
  const pico = useStore((s) => s.pico);
  const stats = useStore((s) => s.stats);
  const clearLogs = useStore((s) => s.clearLogs);

  const [checkingUpdate, setCheckingUpdate] = useState(false);
  const [updateAvailable, setUpdateAvailable] = useState<string | null>(null);

  // Pairing state (only on local ESP32)
  const [pairingData, setPairingData] = useState<PairingData | null>(null);
  const [pairingLoading, setPairingLoading] = useState(false);

  // Fetch pairing QR code data (only on ESP32)
  const fetchPairingData = async () => {
    setPairingLoading(true);
    try {
      const response = await fetch("/api/pairing/qr");
      if (response.ok) {
        const data = await response.json();
        setPairingData(data);
      }
    } catch {
      console.log("Pairing not available");
    }
    setPairingLoading(false);
  };

  const refreshPairing = async () => {
    setPairingLoading(true);
    try {
      const response = await fetch("/api/pairing/refresh", { method: "POST" });
      if (response.ok) {
        const data = await response.json();
        setPairingData(data);
      }
    } catch {
      console.log("Failed to refresh pairing");
    }
    setPairingLoading(false);
  };

  // Fetch pairing data on mount (only on ESP32 local mode)
  useEffect(() => {
    // Only fetch on local ESP32 (not on cloud)
    if (!isGoogleAuthConfigured) {
      fetchPairingData();
    }
  }, []);

  const checkForUpdates = async () => {
    setCheckingUpdate(true);
    getConnection()?.sendCommand("check_update");
    // Simulate check - in real app, wait for response
    setTimeout(() => {
      setCheckingUpdate(false);
      setUpdateAvailable(null);
    }, 2000);
  };

  const startOTA = () => {
    if (
      confirm("Start firmware update? The device will restart after update.")
    ) {
      getConnection()?.sendCommand("ota_start");
    }
  };

  const restartDevice = () => {
    if (confirm("Restart the device?")) {
      getConnection()?.sendCommand("restart");
    }
  };

  const factoryReset = () => {
    if (confirm("This will erase all settings. Are you sure?")) {
      if (confirm("Really? This cannot be undone!")) {
        getConnection()?.sendCommand("factory_reset");
      }
    }
  };

  return (
    <div className="space-y-6">
      {/* Device Info */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* ESP32 Info */}
        <Card>
          <CardHeader>
            <CardTitle icon={<Cpu className="w-5 h-5" />}>ESP32-S3</CardTitle>
            <Badge variant="success">Online</Badge>
          </CardHeader>

          <div className="space-y-3">
            <InfoRow label="Firmware" value={esp32.version || "Unknown"} />
            <InfoRow label="Uptime" value={formatUptime(esp32.uptime)} />
            <InfoRow label="Free Heap" value={formatBytes(esp32.freeHeap)} />
          </div>
        </Card>

        {/* Pico Info */}
        <Card>
          <CardHeader>
            <CardTitle icon={<HardDrive className="w-5 h-5" />}>
              Raspberry Pi Pico
            </CardTitle>
            <Badge variant={pico.connected ? "success" : "error"}>
              {pico.connected ? "Connected" : "Disconnected"}
            </Badge>
          </CardHeader>

          <div className="space-y-3">
            <InfoRow label="Firmware" value={pico.version || "Unknown"} />
            <InfoRow label="Uptime" value={formatUptime(pico.uptime)} />
            <InfoRow
              label="Status"
              value={pico.connected ? "Communicating" : "No response"}
            />
          </div>
        </Card>
      </div>

      {/* Cloud Pairing (only on ESP32 local mode) */}
      {pairingData && (
        <Card>
          <CardHeader>
            <CardTitle icon={<Cloud className="w-5 h-5" />}>
              Cloud Access
            </CardTitle>
          </CardHeader>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            <div>
              <p className="text-sm text-coffee-600 mb-4">
                Link this device to your BrewOS cloud account to control it from
                anywhere. Scan the QR code with your phone or open the link in a
                browser.
              </p>
              <div className="bg-cream-100 rounded-xl p-4">
                <InfoRow label="Device ID" value={pairingData.deviceId} />
              </div>
            </div>

            <QRCodeDisplay
              url={pairingData.url}
              deviceId={pairingData.deviceId}
              expiresIn={pairingData.expiresIn}
              onRefresh={refreshPairing}
              loading={pairingLoading}
            />
          </div>
        </Card>
      )}

      {/* Firmware Update */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Download className="w-5 h-5" />}>
            Firmware Update
          </CardTitle>
        </CardHeader>

        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4">
          <div>
            <p className="text-sm text-coffee-700 mb-1">
              Current version:{" "}
              <span className="font-mono font-semibold">
                {esp32.version || "Unknown"}
              </span>
            </p>
            {updateAvailable ? (
              <p className="text-sm text-emerald-600">
                <Check className="w-4 h-4 inline mr-1" />
                Update available: {updateAvailable}
              </p>
            ) : (
              <p className="text-sm text-coffee-500">
                Check for the latest firmware updates.
              </p>
            )}
          </div>

          <div className="flex gap-3">
            <Button
              variant="secondary"
              onClick={checkForUpdates}
              loading={checkingUpdate}
            >
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

      {/* Statistics */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Clock className="w-5 h-5" />}>Statistics</CardTitle>
        </CardHeader>

        <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
          <StatBox label="Total Shots" value={stats.totalShots} />
          <StatBox label="Today" value={stats.shotsToday} />
          <StatBox
            label="Since Cleaning"
            value={stats.shotsSinceGroupClean}
            warning={stats.shotsSinceGroupClean > 100}
          />
          <StatBox
            label="Last Cleaning"
            value={formatDate(stats.lastGroupCleanTimestamp)}
          />
        </div>
      </Card>

      {/* Time/NTP Settings */}
      <TimeSettings />

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
          <CardTitle icon={<Terminal className="w-5 h-5" />}>
            System Logs
          </CardTitle>
        </CardHeader>

        <LogViewer />
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
            <h4 className="font-semibold text-coffee-800 mb-1">
              Restart Device
            </h4>
            <p className="text-sm text-coffee-500">
              Reboot the ESP32. Settings will be preserved.
            </p>
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
            <p className="text-sm text-coffee-500">
              Erase all settings and return to factory defaults. This cannot be
              undone.
            </p>
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

interface InfoRowProps {
  label: string;
  value: string;
}

function InfoRow({ label, value }: InfoRowProps) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-cream-200 last:border-0">
      <span className="text-sm text-coffee-500">{label}</span>
      <span className="text-sm font-mono font-medium text-coffee-900">
        {value}
      </span>
    </div>
  );
}

interface StatBoxProps {
  label: string;
  value: string | number;
  warning?: boolean;
}

function StatBox({ label, value, warning }: StatBoxProps) {
  return (
    <div className="p-4 bg-cream-100 rounded-xl text-center">
      <div
        className={`text-2xl font-bold ${
          warning ? "text-amber-600" : "text-coffee-900"
        }`}
      >
        {value}
      </div>
      <div className="text-xs text-coffee-500">{label}</div>
    </div>
  );
}

// Common timezone offsets
const TIMEZONES = [
  { offset: -720, label: "UTC-12:00 (Baker Island)" },
  { offset: -660, label: "UTC-11:00 (Samoa)" },
  { offset: -600, label: "UTC-10:00 (Hawaii)" },
  { offset: -540, label: "UTC-09:00 (Alaska)" },
  { offset: -480, label: "UTC-08:00 (Pacific Time)" },
  { offset: -420, label: "UTC-07:00 (Mountain Time)" },
  { offset: -360, label: "UTC-06:00 (Central Time)" },
  { offset: -300, label: "UTC-05:00 (Eastern Time)" },
  { offset: -240, label: "UTC-04:00 (Atlantic Time)" },
  { offset: -180, label: "UTC-03:00 (Buenos Aires)" },
  { offset: -120, label: "UTC-02:00 (Mid-Atlantic)" },
  { offset: -60, label: "UTC-01:00 (Azores)" },
  { offset: 0, label: "UTC+00:00 (London, Dublin)" },
  { offset: 60, label: "UTC+01:00 (Paris, Berlin)" },
  { offset: 120, label: "UTC+02:00 (Jerusalem, Athens)" },
  { offset: 180, label: "UTC+03:00 (Moscow, Istanbul)" },
  { offset: 210, label: "UTC+03:30 (Tehran)" },
  { offset: 240, label: "UTC+04:00 (Dubai)" },
  { offset: 270, label: "UTC+04:30 (Kabul)" },
  { offset: 300, label: "UTC+05:00 (Karachi)" },
  { offset: 330, label: "UTC+05:30 (Mumbai, Delhi)" },
  { offset: 345, label: "UTC+05:45 (Kathmandu)" },
  { offset: 360, label: "UTC+06:00 (Dhaka)" },
  { offset: 420, label: "UTC+07:00 (Bangkok)" },
  { offset: 480, label: "UTC+08:00 (Singapore, Hong Kong)" },
  { offset: 540, label: "UTC+09:00 (Tokyo, Seoul)" },
  { offset: 570, label: "UTC+09:30 (Adelaide)" },
  { offset: 600, label: "UTC+10:00 (Sydney)" },
  { offset: 660, label: "UTC+11:00 (Solomon Islands)" },
  { offset: 720, label: "UTC+12:00 (Auckland)" },
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

function TimeSettings() {
  const [timeStatus, setTimeStatus] = useState<TimeStatus | null>(null);
  const [settings, setSettings] = useState({
    useNTP: true,
    ntpServer: "pool.ntp.org",
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
      const res = await fetch("/api/time");
      if (res.ok) {
        const data = await res.json();
        setTimeStatus(data);
        if (data.settings) {
          setSettings(data.settings);
        }
      }
    } catch (err) {
      console.error("Failed to fetch time status:", err);
    }
  };

  const saveSettings = async () => {
    setSaving(true);
    try {
      const res = await fetch("/api/time", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(settings),
      });
      if (res.ok) {
        await fetchTimeStatus();
      }
    } catch (err) {
      console.error("Failed to save time settings:", err);
    }
    setSaving(false);
  };

  const syncNow = async () => {
    setSyncing(true);
    try {
      await fetch("/api/time/sync", { method: "POST" });
      setTimeout(fetchTimeStatus, 2000); // Fetch status after sync
    } catch (err) {
      console.error("Failed to sync NTP:", err);
    }
    setSyncing(false);
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Globe className="w-5 h-5" />}>
          Time & Timezone
        </CardTitle>
      </CardHeader>

      {/* Current Time Status */}
      <div className="mb-6 p-4 bg-cream-100 rounded-xl">
        <div className="flex items-center justify-between mb-2">
          <span className="text-sm text-coffee-500">Current Time</span>
          <Badge variant={timeStatus?.synced ? "success" : "warning"}>
            {timeStatus?.synced ? "Synced" : "Not synced"}
          </Badge>
        </div>
        <div className="text-2xl font-mono font-bold text-coffee-900">
          {timeStatus?.currentTime || "—"}
        </div>
        <div className="text-sm text-coffee-500 mt-1">
          {timeStatus?.timezone || "Unknown timezone"}
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
          onChange={(e) =>
            setSettings({ ...settings, ntpServer: e.target.value })
          }
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
            onChange={(e) =>
              setSettings({
                ...settings,
                utcOffsetMinutes: parseInt(e.target.value),
              })
            }
            className="input"
          >
            {TIMEZONES.map((tz) => (
              <option key={tz.offset} value={tz.offset}>
                {tz.label}
              </option>
            ))}
          </select>
        </div>

        {/* DST */}
        <div className="flex items-center justify-between p-3 bg-cream-50 rounded-xl">
          <div>
            <div className="font-medium text-coffee-800">
              Daylight Saving Time
            </div>
            <div className="text-sm text-coffee-500">
              Add {settings.dstOffsetMinutes} minutes during DST period
            </div>
          </div>
          <Toggle
            checked={settings.dstEnabled}
            onChange={(enabled) =>
              setSettings({ ...settings, dstEnabled: enabled })
            }
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
            onChange={(e) =>
              setSettings({
                ...settings,
                dstOffsetMinutes: parseInt(e.target.value),
              })
            }
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
