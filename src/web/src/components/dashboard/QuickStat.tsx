import { ReactNode, memo } from "react";
import { Card } from "@/components/Card";
import { cn } from "@/lib/utils";

type StatusType = "success" | "warning" | "error";

const statusColors: Record<StatusType, string> = {
  success: "text-emerald-500",
  warning: "text-amber-500",
  error: "text-red-500",
};

const statusDotColors: Record<StatusType, { bg: string; ping: string }> = {
  success: { bg: "bg-emerald-500", ping: "bg-emerald-400" },
  warning: { bg: "bg-amber-500", ping: "bg-amber-400" },
  error: { bg: "bg-red-500", ping: "bg-red-400" },
};

export interface QuickStatProps {
  icon: ReactNode;
  label: string;
  value: string;
  subtext?: string;
  status?: StatusType;
  showPulse?: boolean;
}

export const QuickStat = memo(function QuickStat({ 
  icon, 
  label, 
  value, 
  subtext,
  status, 
  showPulse 
}: QuickStatProps) {
  const shouldPulse = showPulse && (status === "warning" || status === "error");
  
  return (
    <Card className="flex flex-col items-center justify-between text-center p-4 h-full relative overflow-hidden">
      {/* Status indicator dot - top right corner */}
      {status && (
        <div className="absolute top-2 right-2">
          <span className="relative flex h-2.5 w-2.5">
            {shouldPulse && (
              <span 
                className={cn(
                  "animate-ping absolute inline-flex h-full w-full rounded-full opacity-75",
                  statusDotColors[status].ping
                )} 
              />
            )}
            <span 
              className={cn(
                "relative inline-flex rounded-full h-2.5 w-2.5",
                statusDotColors[status].bg
              )} 
            />
          </span>
        </div>
      )}
      
      <span className="text-accent h-6 flex items-center">{icon}</span>
      <div className="flex flex-col items-center min-h-[2.5rem] justify-center">
        <span
          className={cn(
            "text-lg font-bold leading-tight flex items-center",
            status ? statusColors[status] : "text-theme"
          )}
        >
          {value}
        </span>
        {subtext && (
          <span className="text-[10px] text-theme-muted leading-tight mt-0.5">
            {subtext}
          </span>
        )}
      </div>
      <span className="text-xs text-theme-muted h-4">{label}</span>
    </Card>
  );
});
