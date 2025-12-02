import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Gauge as GaugeIcon } from "lucide-react";

interface PressureCardProps {
  pressure: number;
}

export function PressureCard({ pressure }: PressureCardProps) {
  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<GaugeIcon className="w-5 h-5" />}>Pressure</CardTitle>
      </CardHeader>
      <div className="flex items-baseline gap-2 mb-4">
        <span className="text-5xl font-bold text-accent tabular-nums">
          {pressure.toFixed(1)}
        </span>
        <span className="text-2xl text-theme-muted">bar</span>
      </div>
      <div className="relative h-4 bg-theme-secondary rounded-full overflow-hidden">
        <div
          className="absolute inset-y-0 left-0 rounded-full bg-gradient-to-r from-emerald-400 to-cyan-500 transition-all duration-300"
          style={{ width: `${Math.min(100, (pressure / 15) * 100)}%` }}
        />
        <div className="absolute inset-0 flex justify-between px-1 text-[8px] text-theme-muted">
          {[0, 5, 10, 15].map((mark) => (
            <span key={mark} className="relative top-5">
              {mark}
            </span>
          ))}
        </div>
      </div>
    </Card>
  );
}

