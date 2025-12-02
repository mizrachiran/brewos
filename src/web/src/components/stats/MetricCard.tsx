import { ReactNode } from "react";
import { Card } from "@/components/Card";
import { Badge } from "@/components/Badge";

export type MetricColor = "accent" | "emerald" | "amber" | "blue" | "purple" | "red";

const iconColorClasses: Record<MetricColor, string> = {
  accent: "text-accent",
  emerald: "text-emerald-500",
  amber: "text-amber-500",
  blue: "text-blue-500",
  purple: "text-purple-500",
  red: "text-red-500",
};

export interface MetricCardProps {
  icon: ReactNode;
  label: string;
  value: string | number;
  subtext?: string;
  warning?: boolean;
  color?: MetricColor;
  className?: string;
}

export function MetricCard({
  icon,
  label,
  value,
  subtext,
  warning,
  color = "accent",
  className,
}: MetricCardProps) {
  return (
    <Card className={`flex flex-col ${className || ""}`}>
      <div className="flex items-center gap-2 mb-3">
        <span className={iconColorClasses[color]}>{icon}</span>
        <span className="text-sm font-medium text-theme-muted">{label}</span>
        {warning && (
          <Badge variant="warning" className="ml-auto">
            !
          </Badge>
        )}
      </div>
      <div
        className={`text-3xl font-bold ${
          warning ? "text-amber-500" : "text-theme"
        }`}
      >
        {value}
      </div>
      {subtext && (
        <div className="text-xs text-theme-muted mt-1">{subtext}</div>
      )}
    </Card>
  );
}

