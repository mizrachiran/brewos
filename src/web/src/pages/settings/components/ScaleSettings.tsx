import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Button } from '@/components/Button';
import { Badge } from '@/components/Badge';
import {
  Scale as ScaleIcon,
  Bluetooth,
  Battery,
  Loader2,
  Signal,
  Check,
  X,
} from 'lucide-react';

export function ScaleSettings() {
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

