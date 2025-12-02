import { ReactNode } from "react";

interface SettingsSectionProps {
  title?: string;
  description?: string;
  children: ReactNode;
  className?: string;
}

export function SettingsSection({
  title,
  description,
  children,
  className = "",
}: SettingsSectionProps) {
  return (
    <div className={`space-y-4 ${className}`}>
      {(title || description) && (
        <div className="mb-4">
          {title && (
            <h3 className="text-sm font-semibold text-theme mb-1">{title}</h3>
          )}
          {description && (
            <p className="text-sm text-theme-muted">{description}</p>
          )}
        </div>
      )}
      {children}
    </div>
  );
}

