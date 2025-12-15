import { ReactNode } from "react";
import { LucideIcon } from "lucide-react";

interface StatsCardProps {
  title: string;
  value: string | number;
  subtitle?: string;
  icon: LucideIcon;
  trend?: {
    value: string | number;
    positive: boolean;
  };
  className?: string;
}

export function StatsCard({
  title,
  value,
  subtitle,
  icon: Icon,
  trend,
  className = "",
}: StatsCardProps) {
  return (
    <div
      className={`admin-card group hover:shadow-admin-hover transition-all duration-300 ${className}`}
    >
      <div className="flex items-start justify-between">
        <div>
          <p className="text-admin-text-secondary text-sm mb-1">{title}</p>
          <p className="text-3xl font-bold text-admin-text font-display">
            {value}
          </p>
          {subtitle && (
            <p className="text-admin-text-secondary text-xs mt-1">{subtitle}</p>
          )}
          {trend && (
            <p
              className={`text-xs mt-2 ${
                trend.positive ? "text-admin-success" : "text-admin-danger"
              }`}
            >
              {trend.positive ? "↑" : "↓"} {trend.value}
            </p>
          )}
        </div>
        <div className="w-12 h-12 rounded-xl bg-admin-accent/10 flex items-center justify-center group-hover:bg-admin-accent/20 transition-colors">
          <Icon className="w-6 h-6 text-admin-accent" />
        </div>
      </div>
    </div>
  );
}

// Simplified stats card for inline display
interface MiniStatsProps {
  label: string;
  value: string | number;
  icon?: ReactNode;
}

export function MiniStats({ label, value, icon }: MiniStatsProps) {
  return (
    <div className="flex items-center gap-2 px-3 py-2 bg-admin-surface rounded-lg">
      {icon}
      <span className="text-admin-text-secondary text-sm">{label}:</span>
      <span className="text-admin-text font-medium">{value}</span>
    </div>
  );
}

