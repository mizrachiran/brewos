import { Button } from "@/components/Button";
import { Badge } from "@/components/Badge";
import { formatDate } from "@/lib/date";

export interface MaintenanceCardProps {
  label: string;
  shotsSince: number;
  lastTimestamp: number;
  threshold: number;
  onMark: () => void;
}

export function MaintenanceCard({
  label,
  shotsSince,
  lastTimestamp,
  threshold,
  onMark,
}: MaintenanceCardProps) {
  const isOverdue = shotsSince >= threshold;
  const lastDate =
    lastTimestamp > 0 ? formatDate(lastTimestamp, { dateStyle: "short" }) : "Never";

  return (
    <div
      className={`p-4 rounded-xl ${
        isOverdue
          ? "bg-amber-500/10 border border-amber-500/30"
          : "bg-theme-secondary"
      }`}
    >
      <div className="flex items-center justify-between mb-2">
        <span className="text-sm font-medium text-theme-secondary">{label}</span>
        {isOverdue && <Badge variant="warning">Due</Badge>}
      </div>
      <div
        className={`text-2xl font-bold ${isOverdue ? "text-amber-500" : "text-theme"}`}
      >
        {shotsSince}
      </div>
      <div className="text-xs text-theme-muted mb-3">shots since last</div>
      <div className="flex items-center justify-between">
        <span className="text-xs text-theme-muted">Last: {lastDate}</span>
        <Button
          size="sm"
          variant={isOverdue ? "primary" : "secondary"}
          onClick={onMark}
        >
          Mark Done
        </Button>
      </div>
    </div>
  );
}

