import { memo, useMemo } from "react";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { useStore } from "@/lib/store";
import { Zap, TrendingUp, Gauge, Activity, DollarSign } from "lucide-react";

interface PowerCardProps {
  current: number;
  todayKwh: number;
  voltage: number;
}

// Electricity price estimation (configurable in future)
const PRICE_PER_KWH = 0.15; // USD/kWh - rough average

export const PowerCard = memo(function PowerCard({ current, todayKwh, voltage }: PowerCardProps) {
  const maxCurrent = useStore((s) => s.power.maxCurrent);
  const totalKwh = useStore((s) => s.power.totalKwh);
  const isHeating = useStore((s) => s.machine.isHeating);
  const isBrewing = useStore((s) => s.machine.isBrewing);

  // Calculate power percentage of max
  const maxWatts = (maxCurrent || 13) * voltage;
  const powerPercentage = Math.min(100, (current / maxWatts) * 100);

  // Determine power state and color
  const powerState = useMemo(() => {
    if (current < 10) return { label: "Idle", color: "text-theme-muted", bg: "bg-theme-secondary" };
    if (isHeating) return { label: "Heating", color: "text-orange-500", bg: "bg-orange-500" };
    if (isBrewing) return { label: "Brewing", color: "text-cyan-500", bg: "bg-cyan-500" };
    if (current > 100) return { label: "Active", color: "text-emerald-500", bg: "bg-emerald-500" };
    return { label: "Standby", color: "text-amber-500", bg: "bg-amber-500" };
  }, [current, isHeating, isBrewing]);

  // Estimated cost today
  const todayCost = (todayKwh * PRICE_PER_KWH).toFixed(2);

  return (
    <Card>
      <CardHeader
        action={
          <span className={`text-xs font-medium px-2 py-1 rounded-full ${powerState.bg}/20 ${powerState.color}`}>
            {powerState.label}
          </span>
        }
      >
        <CardTitle icon={<Zap className="w-5 h-5" />}>Power</CardTitle>
      </CardHeader>

      {/* Main power display */}
      <div className="flex items-baseline gap-2 mb-3">
        <span className="text-4xl font-bold text-accent tabular-nums">
          {Math.round(current)}
        </span>
        <span className="text-xl text-theme-muted">W</span>
      </div>

      {/* Power bar */}
      <div className="mb-4">
        <div className="flex items-center justify-between text-xs text-theme-muted mb-1">
          <span>Power usage</span>
          <span>{powerPercentage.toFixed(0)}%</span>
        </div>
        <div className="h-2 bg-theme-secondary rounded-full overflow-hidden">
          <div
            className={`h-full rounded-full transition-all duration-300 ${
              powerPercentage > 80 ? 'bg-orange-500' : powerPercentage > 50 ? 'bg-amber-500' : 'bg-emerald-500'
            }`}
            style={{ width: `${powerPercentage}%` }}
          />
        </div>
      </div>

      {/* Stats grid */}
      <div className="grid grid-cols-2 gap-3">
        <StatItem
          icon={<Activity className="w-4 h-4" />}
          label="Today"
          value={`${todayKwh.toFixed(2)} kWh`}
        />
        <StatItem
          icon={<DollarSign className="w-4 h-4" />}
          label="Est. Cost"
          value={`$${todayCost}`}
        />
        <StatItem
          icon={<Gauge className="w-4 h-4" />}
          label="Voltage"
          value={`${voltage} V`}
        />
        <StatItem
          icon={<TrendingUp className="w-4 h-4" />}
          label="Lifetime"
          value={`${totalKwh.toFixed(1)} kWh`}
        />
      </div>
    </Card>
  );
});

interface StatItemProps {
  icon: React.ReactNode;
  label: string;
  value: string;
}

function StatItem({ icon, label, value }: StatItemProps) {
  return (
    <div className="flex items-start gap-2 p-2 rounded-lg bg-theme-secondary">
      <div className="text-theme-muted mt-0.5">{icon}</div>
      <div>
        <div className="text-xs text-theme-muted">{label}</div>
        <div className="text-sm font-semibold text-theme">{value}</div>
      </div>
    </div>
  );
}
