import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Zap } from "lucide-react";

interface PowerCardProps {
  current: number;
  todayKwh: number;
  voltage: number;
}

export function PowerCard({ current, todayKwh, voltage }: PowerCardProps) {
  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Zap className="w-5 h-5" />}>Power</CardTitle>
      </CardHeader>
      <div className="flex items-baseline gap-2 mb-4">
        <span className="text-5xl font-bold text-accent tabular-nums">
          {Math.round(current)}
        </span>
        <span className="text-2xl text-theme-muted">W</span>
      </div>
      <div className="grid grid-cols-2 gap-4 text-sm">
        <div>
          <span className="text-theme-muted">Today</span>
          <p className="font-semibold text-theme-secondary">
            {todayKwh.toFixed(1)} kWh
          </p>
        </div>
        <div>
          <span className="text-theme-muted">Voltage</span>
          <p className="font-semibold text-theme-secondary">{voltage} V</p>
        </div>
      </div>
    </Card>
  );
}
