import { ReactNode } from "react";
import { cn } from "@/lib/utils";
import { useMobileLandscape } from "@/lib/useMobileLandscape";

interface PageHeaderProps {
  title: string;
  subtitle?: string;
  action?: ReactNode;
  className?: string;
}

export function PageHeader({
  title,
  subtitle,
  action,
  className,
}: PageHeaderProps) {
  const isMobileLandscape = useMobileLandscape();

  // In landscape mode, title is shown in the Layout header, so only show action
  if (isMobileLandscape) {
    return action ? (
      <div className={cn("flex justify-end", className)}>{action}</div>
    ) : null;
  }

  return (
    <div className={cn("flex items-center justify-between", className)}>
      <div>
        <h1 className="text-2xl font-bold text-theme">{title}</h1>
        {subtitle && <p className="text-theme-muted mt-1">{subtitle}</p>}
      </div>
      {action && <div>{action}</div>}
    </div>
  );
}
