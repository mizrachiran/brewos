import { ReactNode } from "react";

export interface StatRowProps {
  label: string;
  value: string;
  icon?: ReactNode;
  mono?: boolean;
}

export function StatRow({ label, value, icon, mono }: StatRowProps) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-theme last:border-0">
      <div className="flex items-center gap-2 text-theme-muted">
        {icon}
        <span className="text-sm">{label}</span>
      </div>
      <span className={`text-sm font-semibold text-theme ${mono ? "font-mono" : ""}`}>
        {value}
      </span>
    </div>
  );
}

