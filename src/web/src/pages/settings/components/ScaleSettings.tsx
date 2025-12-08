import { useStore } from '@/lib/store';
import { useCommand } from '@/lib/useCommand';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Button } from '@/components/Button';
import { Badge } from '@/components/Badge';
import { ScaleStatusCard } from '@/components/ScaleStatusCard';
import {
  Scale as ScaleIcon,
  Bluetooth,
  Loader2,
  Signal,
  X,
  ChevronRight,
  RotateCcw,
  Unplug,
} from 'lucide-react';

export function ScaleSettings() {
  const scale = useStore((s) => s.scale);
  const scanning = useStore((s) => s.scaleScanning);
  const scanResults = useStore((s) => s.scanResults);
  const setScanning = useStore((s) => s.setScaleScanning);
  const clearResults = useStore((s) => s.clearScanResults);
  const { sendCommand } = useCommand();

  const startScan = () => {
    clearResults();
    setScanning(true);
    sendCommand('scale_scan');
  };

  const stopScan = () => {
    setScanning(false);
    sendCommand('scale_scan_stop');
  };

  const connectScale = (address: string) => {
    if (sendCommand('scale_connect', { address }, { successMessage: 'Connecting to scale...' })) {
      setScanning(false);
    }
  };

  const disconnectScale = () => {
    sendCommand('scale_disconnect', undefined, { successMessage: 'Scale disconnected' });
  };

  const tareScale = () => {
    sendCommand('tare');
  };

  return (
    <div className="space-y-6">
      {/* Connected Scale - Using shared component */}
      {scale.connected && (
        <ScaleStatusCard
          compact
          actions={
            <div className="space-y-0 border-t border-emerald-500/20">
              <button
                onClick={tareScale}
                className="w-full flex items-center justify-between py-2.5 border-b border-emerald-500/20 text-left group transition-colors hover:opacity-80"
              >
                <div className="flex items-center gap-3">
                  <RotateCcw className="w-4 h-4 text-emerald-600 dark:text-emerald-400" />
                  <span className="text-sm font-medium text-emerald-900 dark:text-emerald-100">Tare Scale</span>
                </div>
                <ChevronRight className="w-4 h-4 text-emerald-600/50 dark:text-emerald-400/50 group-hover:text-emerald-600 dark:group-hover:text-emerald-400 transition-colors" />
              </button>
              <button
                onClick={disconnectScale}
                className="w-full flex items-center justify-between py-2.5 text-left group transition-colors hover:opacity-80"
              >
                <div className="flex items-center gap-3">
                  <Unplug className="w-4 h-4 text-emerald-600 dark:text-emerald-400" />
                  <span className="text-sm font-medium text-emerald-900 dark:text-emerald-100">Disconnect Scale</span>
                </div>
                <ChevronRight className="w-4 h-4 text-emerald-600/50 dark:text-emerald-400/50 group-hover:text-emerald-600 dark:group-hover:text-emerald-400 transition-colors" />
              </button>
            </div>
          }
        />
      )}

      {/* Scan for Scales */}
      {!scale.connected && (
        <Card>
          <div className="text-center py-8">
            <div className="w-20 h-20 bg-theme-secondary rounded-full flex items-center justify-center mx-auto mb-4">
              <ScaleIcon className="w-10 h-10 text-theme-muted" />
            </div>
            <h2 className="text-xl font-bold text-theme mb-2">No Scale Connected</h2>
            <p className="text-theme-muted mb-6">
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
                  className="flex items-center justify-between p-4 bg-theme-secondary rounded-xl"
                >
                  <div className="flex items-center gap-3">
                    <div className="w-10 h-10 bg-theme-tertiary rounded-full flex items-center justify-center">
                      <ScaleIcon className="w-5 h-5 text-theme-muted" />
                    </div>
                    <div>
                      <div className="font-semibold text-theme">
                        {result.name || 'Unknown Scale'}
                      </div>
                      <div className="text-xs text-theme-muted flex items-center gap-2">
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
            <div className="flex items-center justify-center gap-3 py-8 text-theme-muted">
              <Loader2 className="w-5 h-5 animate-spin" />
              <span>Searching for scales...</span>
            </div>
          ) : (
            <p className="text-center text-theme-muted py-8">
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
              className="p-4 bg-theme-secondary rounded-xl"
            >
              <div className="font-semibold text-theme">{item.brand}</div>
              <div className="text-sm text-theme-muted">{item.models}</div>
            </div>
          ))}
        </div>
      </Card>
    </div>
  );
}
