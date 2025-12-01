import { useState, useEffect } from 'react';
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
  FlaskConical,
  Shield,
  ExternalLink,
  Info,
} from 'lucide-react';
import { StatusRow } from './StatusRow';
import {
  checkForUpdates,
  getUpdateChannel,
  setUpdateChannel,
  getVersionDisplay,
  formatReleaseDate,
  type UpdateChannel,
  type UpdateCheckResult,
} from '@/lib/updates';

export function SystemSettings() {
  const esp32 = useStore((s) => s.esp32);
  const pico = useStore((s) => s.pico);
  const logs = useStore((s) => s.logs);
  const clearLogs = useStore((s) => s.clearLogs);

  const [checkingUpdate, setCheckingUpdate] = useState(false);
  const [updateResult, setUpdateResult] = useState<UpdateCheckResult | null>(null);
  const [channel, setChannel] = useState<UpdateChannel>(() => getUpdateChannel());
  const [showBetaWarning, setShowBetaWarning] = useState(false);

  // Check for updates on mount or when channel changes
  useEffect(() => {
    if (esp32.version) {
      handleCheckForUpdates();
    }
  }, [esp32.version, channel]);

  const handleCheckForUpdates = async () => {
    if (!esp32.version) return;
    
    setCheckingUpdate(true);
    try {
      const result = await checkForUpdates(esp32.version);
      setUpdateResult(result);
    } catch (error) {
      console.error('Failed to check for updates:', error);
    } finally {
      setCheckingUpdate(false);
    }
  };

  const handleChannelChange = (newChannel: UpdateChannel) => {
    if (newChannel === 'beta' && channel === 'stable') {
      setShowBetaWarning(true);
    } else {
      setChannel(newChannel);
      setUpdateChannel(newChannel);
    }
  };

  const confirmBetaChannel = () => {
    setChannel('beta');
    setUpdateChannel('beta');
    setShowBetaWarning(false);
  };

  const startOTA = (version: string) => {
    const isBeta = version.includes('-');
    const warningText = isBeta 
      ? `Install BETA version ${version}? This is a pre-release version for testing. The device will restart after update.`
      : `Install version ${version}? The device will restart after update.`;
    
    if (confirm(warningText)) {
      getConnection()?.sendCommand('ota_start', { version });
    }
  };

  const currentVersionDisplay = getVersionDisplay(esp32.version || '0.0.0');

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

        {/* Current Version */}
        <div className="flex items-center gap-3 mb-6 p-4 bg-theme rounded-xl">
          <div className="flex-1">
            <p className="text-xs text-theme-muted uppercase tracking-wider mb-1">Installed Version</p>
            <div className="flex items-center gap-2">
              <span className="font-mono text-lg font-bold text-theme">{esp32.version || 'Unknown'}</span>
              {currentVersionDisplay.badge && (
                <Badge variant={currentVersionDisplay.badge === 'stable' ? 'success' : 'warning'}>
                  {currentVersionDisplay.badge === 'stable' ? (
                    <><Shield className="w-3 h-3" /> Official</>
                  ) : (
                    <><FlaskConical className="w-3 h-3" /> {currentVersionDisplay.badge.toUpperCase()}</>
                  )}
                </Badge>
              )}
            </div>
          </div>
          <Button variant="secondary" size="sm" onClick={handleCheckForUpdates} loading={checkingUpdate}>
            <RefreshCw className="w-4 h-4" />
            Check
          </Button>
        </div>

        {/* Update Channel Selection */}
        <div className="mb-6">
          <label className="text-sm font-medium text-theme mb-3 block">Update Channel</label>
          <div className="grid grid-cols-2 gap-3">
            <button
              onClick={() => handleChannelChange('stable')}
              className={`p-4 rounded-xl border-2 transition-all text-left ${
                channel === 'stable'
                  ? 'border-emerald-500 bg-emerald-500/10'
                  : 'border-theme hover:border-theme-light'
              }`}
            >
              <div className="flex items-center gap-2 mb-1">
                <Shield className={`w-5 h-5 ${channel === 'stable' ? 'text-emerald-500' : 'text-theme-muted'}`} />
                <span className={`font-semibold ${channel === 'stable' ? 'text-emerald-600' : 'text-theme'}`}>
                  Stable
                </span>
              </div>
              <p className="text-xs text-theme-muted">
                Recommended for most users. Tested and reliable.
              </p>
            </button>

            <button
              onClick={() => handleChannelChange('beta')}
              className={`p-4 rounded-xl border-2 transition-all text-left ${
                channel === 'beta'
                  ? 'border-amber-500 bg-amber-500/10'
                  : 'border-theme hover:border-theme-light'
              }`}
            >
              <div className="flex items-center gap-2 mb-1">
                <FlaskConical className={`w-5 h-5 ${channel === 'beta' ? 'text-amber-500' : 'text-theme-muted'}`} />
                <span className={`font-semibold ${channel === 'beta' ? 'text-amber-600' : 'text-theme'}`}>
                  Beta
                </span>
              </div>
              <p className="text-xs text-theme-muted">
                Get new features early. May contain bugs.
              </p>
            </button>
          </div>
        </div>

        {/* Available Updates */}
        {updateResult && (
          <div className="space-y-4">
            {/* Stable Update */}
            {updateResult.stable && (
              <div className={`p-4 rounded-xl border ${
                updateResult.hasStableUpdate 
                  ? 'border-emerald-200 bg-emerald-50' 
                  : 'border-theme bg-theme'
              }`}>
                <div className="flex items-start justify-between gap-4">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <Shield className="w-4 h-4 text-emerald-500" />
                      <span className="font-semibold text-theme">Official Release</span>
                      <Badge variant="success">
                        v{updateResult.stable.version}
                      </Badge>
                    </div>
                    <p className="text-xs text-theme-muted mb-2">
                      Released {formatReleaseDate(updateResult.stable.releaseDate)}
                    </p>
                    {updateResult.hasStableUpdate ? (
                      <p className="text-sm text-emerald-600 flex items-center gap-1">
                        <Check className="w-4 h-4" />
                        Update available
                      </p>
                    ) : (
                      <p className="text-sm text-theme-muted">
                        You're on the latest stable version
                      </p>
                    )}
                  </div>
                  <div className="flex flex-col gap-2">
                    {updateResult.hasStableUpdate && (
                      <Button size="sm" onClick={() => startOTA(updateResult.stable!.version)}>
                        <Download className="w-4 h-4" />
                        Install
                      </Button>
                    )}
                    {updateResult.stable.downloadUrl && (
                      <a
                        href={updateResult.stable.downloadUrl}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-accent hover:underline flex items-center gap-1"
                      >
                        Release Notes <ExternalLink className="w-3 h-3" />
                      </a>
                    )}
                  </div>
                </div>
              </div>
            )}

            {/* Beta Update - only show if user is on beta channel */}
            {channel === 'beta' && updateResult.beta && (
              <div className={`p-4 rounded-xl border ${
                updateResult.hasBetaUpdate 
                  ? 'border-amber-200 bg-amber-50' 
                  : 'border-theme bg-theme'
              }`}>
                <div className="flex items-start justify-between gap-4">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <FlaskConical className="w-4 h-4 text-amber-500" />
                      <span className="font-semibold text-theme">Beta Release</span>
                      <Badge variant="warning">
                        v{updateResult.beta.version}
                      </Badge>
                    </div>
                    <p className="text-xs text-theme-muted mb-2">
                      Released {formatReleaseDate(updateResult.beta.releaseDate)}
                    </p>
                    {updateResult.hasBetaUpdate ? (
                      <p className="text-sm text-amber-600 flex items-center gap-1">
                        <Check className="w-4 h-4" />
                        New beta available
                      </p>
                    ) : (
                      <p className="text-sm text-theme-muted">
                        You're on the latest beta version
                      </p>
                    )}
                    
                    {/* Beta Warning */}
                    <div className="mt-3 p-2 bg-amber-100 rounded-lg flex items-start gap-2">
                      <AlertTriangle className="w-4 h-4 text-amber-600 flex-shrink-0 mt-0.5" />
                      <p className="text-xs text-amber-700">
                        Beta versions are for testing. They may contain bugs or incomplete features.
                      </p>
                    </div>
                  </div>
                  <div className="flex flex-col gap-2">
                    {updateResult.hasBetaUpdate && (
                      <Button size="sm" variant="secondary" onClick={() => startOTA(updateResult.beta!.version)}>
                        <Download className="w-4 h-4" />
                        Install Beta
                      </Button>
                    )}
                    {updateResult.beta.downloadUrl && (
                      <a
                        href={updateResult.beta.downloadUrl}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-accent hover:underline flex items-center gap-1"
                      >
                        Release Notes <ExternalLink className="w-3 h-3" />
                      </a>
                    )}
                  </div>
                </div>
              </div>
            )}

            {/* No updates available message */}
            {!updateResult.hasStableUpdate && (channel !== 'beta' || !updateResult.hasBetaUpdate) && (
              <div className="text-center py-4 text-theme-muted">
                <Check className="w-8 h-8 mx-auto mb-2 text-emerald-500" />
                <p className="text-sm">You're running the latest version!</p>
              </div>
            )}
          </div>
        )}
      </Card>

      {/* Beta Warning Modal */}
      {showBetaWarning && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm p-4">
          <Card className="w-full max-w-md">
            <div className="text-center p-6">
              <div className="w-16 h-16 bg-amber-100 rounded-full flex items-center justify-center mx-auto mb-4">
                <FlaskConical className="w-8 h-8 text-amber-600" />
              </div>
              <h3 className="text-xl font-bold text-theme mb-2">Enable Beta Updates?</h3>
              <p className="text-sm text-theme-muted mb-4">
                Beta versions include new features before they're officially released. 
                They may contain bugs or incomplete features.
              </p>
              <div className="p-3 bg-amber-50 rounded-lg mb-6 text-left">
                <h4 className="font-semibold text-amber-800 text-sm mb-2 flex items-center gap-2">
                  <Info className="w-4 h-4" /> What to expect:
                </h4>
                <ul className="text-xs text-amber-700 space-y-1">
                  <li>• Early access to new features</li>
                  <li>• Potential bugs or stability issues</li>
                  <li>• More frequent updates</li>
                  <li>• You can switch back to stable anytime</li>
                </ul>
              </div>
              <div className="flex gap-3">
                <Button variant="secondary" className="flex-1" onClick={() => setShowBetaWarning(false)}>
                  Cancel
                </Button>
                <Button className="flex-1" onClick={confirmBetaChannel}>
                  <FlaskConical className="w-4 h-4" />
                  Enable Beta
                </Button>
              </div>
            </div>
          </Card>
        </div>
      )}

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

