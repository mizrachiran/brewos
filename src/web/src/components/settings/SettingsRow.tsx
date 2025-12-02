import { ReactNode } from "react";

interface SettingsRowProps {
  label: string;
  description?: string;
  children: ReactNode;
  vertical?: boolean;
}

export function SettingsRow({
  label,
  description,
  children,
  vertical = false,
}: SettingsRowProps) {
  if (vertical) {
    return (
      <div className="space-y-2">
        <div>
          <label className="block text-sm font-medium text-theme">{label}</label>
          {description && (
            <p className="text-xs text-theme-muted mt-0.5">{description}</p>
          )}
        </div>
        {children}
      </div>
    );
  }

  return (
    <div className="flex items-center justify-between py-3 border-b border-theme last:border-0">
      <div className="flex-1 min-w-0 pr-4">
        <span className="block text-sm font-medium text-theme">{label}</span>
        {description && (
          <span className="block text-xs text-theme-muted mt-0.5">{description}</span>
        )}
      </div>
      <div className="flex-shrink-0">{children}</div>
    </div>
  );
}

