import { Badge } from "@/components/Badge";

export interface MilestoneCardProps {
  label: string;
  achieved: boolean;
  icon: string;
}

export function MilestoneCard({ label, achieved, icon }: MilestoneCardProps) {
  return (
    <div
      className={`p-4 rounded-xl text-center transition-all ${
        achieved
          ? "bg-gradient-to-br from-accent/10 to-accent/5 border border-accent/20"
          : "bg-theme-secondary opacity-50"
      }`}
    >
      <div className="text-2xl mb-2">{icon}</div>
      <div
        className={`text-sm font-medium ${
          achieved ? "text-theme" : "text-theme-muted"
        }`}
      >
        {label}
      </div>
      {achieved && (
        <Badge variant="success" className="mt-2">
          Achieved
        </Badge>
      )}
    </div>
  );
}

