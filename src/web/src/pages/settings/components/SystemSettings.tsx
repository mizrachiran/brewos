import React, { useState } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Button } from '@/components/Button';
import { Badge } from '@/components/Badge';
import { formatUptime, formatBytes } from '@/lib/utils';
import {
  Cpu,
  HardDrive,
  Download,
  Terminal,
  Trash2,
  AlertTriangle,
  RefreshCw,
  Check,
} from 'lucide-react';
import { StatusRow } from './StatusRow';

export function SystemSettings() {
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

