import { ReactNode } from "react";
import { Card } from "@/components/Card";

type StatusType = "success" | "warning" | "error";

const statusColors: Record<StatusType, string> = {
  success: "text-emerald-500",
  warning: "text-amber-500",
  error: "text-red-500",
};

export interface QuickStatProps {
  icon: ReactNode;
  label: string;
  value: string;
  status?: StatusType;
}

export function QuickStat({ icon, label, value, status }: QuickStatProps) {
  return (
    <Card className="flex flex-col items-center justify-between text-center p-4 h-full">
      <span className="text-accent h-6 flex items-center">{icon}</span>
      <span
        className={`text-lg font-bold leading-tight min-h-[1.75rem] flex items-center ${
          status ? statusColors[status] : "text-theme"
        }`}
      >
        {value}
      </span>
      <span className="text-xs text-theme-muted h-4">{label}</span>
    </Card>
  );
}

