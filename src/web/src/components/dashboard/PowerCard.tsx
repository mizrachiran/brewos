import { memo, useMemo } from "react";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { useStore } from "@/lib/store";
import { Zap, TrendingUp, Gauge, Activity } from "lucide-react";
import type { Currency } from "@/lib/types";

interface PowerCardProps {
  current: number;
  todayKwh: number;
  voltage: number;
}

// Currency symbols map
const CURRENCY_SYMBOLS: Record<Currency, string> = {
  USD: '$',
  EUR: '€',
  GBP: '£',
  AUD: 'A$',
  CAD: 'C$',
  JPY: '¥',
  CHF: 'Fr',
  ILS: '₪',
};

export const PowerCard = memo(function PowerCard({ current, todayKwh, voltage }: PowerCardProps) {
  const totalKwh = useStore((s) => s.power.totalKwh);
  const isHeating = useStore((s) => s.machine.isHeating);
  const isBrewing = useStore((s) => s.machine.isBrewing);
  const { electricityPrice, currency } = useStore((s) => s.preferences);

  // Determine power state and color
  const powerState = useMemo(() => {
    if (current < 10) return { label: "Idle", color: "text-theme-muted", bg: "bg-theme-secondary", barColor: "bg-theme-muted" };
    if (isHeating) return { label: "Heating", color: "text-orange-500", bg: "bg-orange-500", barColor: "bg-orange-500" };
    if (isBrewing) return { label: "Brewing", color: "text-cyan-500", bg: "bg-cyan-500", barColor: "bg-cyan-500" };
    if (current > 100) return { label: "Active", color: "text-emerald-500", bg: "bg-emerald-500", barColor: "bg-emerald-500" };
    return { label: "Standby", color: "text-amber-500", bg: "bg-amber-500", barColor: "bg-amber-500" };
  }, [current, isHeating, isBrewing]);

  // Get currency symbol
  const currencySymbol = CURRENCY_SYMBOLS[currency] || '$';

  // Estimated cost today using user's configured price
  const todayCost = (todayKwh * electricityPrice).toFixed(2);

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

      {/* Main power display with animated bar */}
      <div className="flex items-baseline gap-2 mb-2">
        <span className="text-4xl font-bold text-accent tabular-nums">
          {Math.round(current)}
        </span>
        <span className="text-xl text-theme-muted">W</span>
      </div>

      {/* Simple state indicator bar */}
      <div className="h-1.5 bg-theme-secondary rounded-full overflow-hidden mb-4">
        <div
          className={`h-full rounded-full transition-all duration-500 ${powerState.barColor} ${
            isHeating ? 'animate-pulse' : ''
          }`}
          style={{ width: current < 10 ? '5%' : current < 100 ? '30%' : current < 500 ? '50%' : current < 1000 ? '70%' : '100%' }}
        />
      </div>

      {/* Stats grid */}
      <div className="grid grid-cols-2 gap-3">
        <StatItem
          icon={<Activity className="w-4 h-4" />}
          label="Today"
          value={`${todayKwh.toFixed(2)} kWh`}
        />
        <StatItem
          icon={<span className="text-sm font-bold">{currencySymbol}</span>}
          label="Today's Cost"
          value={`${currencySymbol}${todayCost}`}
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
