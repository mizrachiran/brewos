import { useStore } from "@/lib/store";
import { Activity, Zap, Gauge, Battery, Radio } from "lucide-react";

function formatLastUpdate(timestamp: number | null): string {
  if (!timestamp) return "Never";
  
  const secondsAgo = Math.floor((Date.now() - timestamp) / 1000);
  
  if (secondsAgo < 10) return "Just now";
  if (secondsAgo < 60) return `${secondsAgo} seconds ago`;
  
  const minutesAgo = Math.floor(secondsAgo / 60);
  if (minutesAgo < 60) return `${minutesAgo} minute${minutesAgo > 1 ? 's' : ''} ago`;
  
  const hoursAgo = Math.floor(minutesAgo / 60);
  return `${hoursAgo} hour${hoursAgo > 1 ? 's' : ''} ago`;
}

export function PowerMeterStatus() {
  const powerMeter = useStore((s) => s.power.meter);

  if (!powerMeter?.reading) {
    return null;
  }

  const reading = powerMeter.reading;
  const lastUpdate = formatLastUpdate(powerMeter.lastUpdate);

  return (
    <div className="space-y-0 pt-2 border-t border-theme">
      <div className="pb-2">
        <p className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
          Current Readings
        </p>
        <p className="text-xs text-theme-muted mt-0.5">
          Updated {lastUpdate}
        </p>
      </div>

      {/* Voltage */}
      <div className="flex items-center justify-between py-1.5">
        <div className="flex items-center gap-2">
          <Zap className="w-4 h-4 text-theme-muted" />
          <span className="text-sm text-theme-muted">Voltage</span>
        </div>
        <span className="text-sm font-medium text-theme">
          {reading.voltage.toFixed(1)} V
        </span>
      </div>

      {/* Current */}
      <div className="flex items-center justify-between py-1.5">
        <div className="flex items-center gap-2">
          <Activity className="w-4 h-4 text-theme-muted" />
          <span className="text-sm text-theme-muted">Current</span>
        </div>
        <span className="text-sm font-medium text-theme">
          {reading.current.toFixed(2)} A
        </span>
      </div>

      {/* Power */}
      <div className="flex items-center justify-between py-1.5">
        <div className="flex items-center gap-2">
          <Battery className="w-4 h-4 text-theme-muted" />
          <span className="text-sm text-theme-muted">Power</span>
        </div>
        <span className="text-sm font-medium text-theme">
          {reading.power.toFixed(0)} W
        </span>
      </div>

      {/* Energy */}
      <div className="flex items-center justify-between py-1.5">
        <div className="flex items-center gap-2">
          <Gauge className="w-4 h-4 text-theme-muted" />
          <span className="text-sm text-theme-muted">Energy Total</span>
        </div>
        <span className="text-sm font-medium text-theme">
          {reading.energy.toFixed(2)} kWh
        </span>
      </div>

      {/* Frequency */}
      {reading.frequency > 0 && (
        <div className="flex items-center justify-between py-1.5">
          <div className="flex items-center gap-2">
            <Radio className="w-4 h-4 text-theme-muted" />
            <span className="text-sm text-theme-muted">Frequency</span>
          </div>
          <span className="text-sm font-medium text-theme">
            {reading.frequency.toFixed(1)} Hz
          </span>
        </div>
      )}

      {/* Power Factor */}
      {reading.powerFactor > 0 && (
        <div className="flex items-center justify-between py-1.5">
          <span className="text-sm text-theme-muted">Power Factor</span>
          <span className="text-sm font-medium text-theme">
            {reading.powerFactor.toFixed(2)}
          </span>
        </div>
      )}
    </div>
  );
}

