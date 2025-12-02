import { memo } from 'react';
import { cn } from '@/lib/utils';
import { useStore } from '@/lib/store';
import { getUnitSymbol, convertFromCelsius } from '@/lib/temperature';

interface GaugeProps {
  value: number;           // Always in Celsius from backend
  max: number;             // Always in Celsius
  setpoint?: number;       // Always in Celsius
  label: string;
  unit?: string;           // Override unit symbol (for non-temperature gauges)
  icon?: React.ReactNode;
  variant?: 'default' | 'steam' | 'pressure';
  showSetpoint?: boolean;
  isTemperature?: boolean; // Set to false for pressure, etc.
}

export const Gauge = memo(function Gauge({
  value,
  max,
  setpoint,
  label,
  unit,
  icon,
  variant = 'default',
  showSetpoint = true,
  isTemperature = true,
}: GaugeProps) {
  const temperatureUnit = useStore((s) => s.preferences.temperatureUnit);
  
  // Convert values if this is a temperature gauge
  const displayValue = isTemperature ? convertFromCelsius(value, temperatureUnit) : value;
  const displaySetpoint = setpoint && isTemperature ? convertFromCelsius(setpoint, temperatureUnit) : setpoint;
  
  // Use appropriate unit symbol
  const displayUnit = unit || (isTemperature ? getUnitSymbol(temperatureUnit) : '');
  
  // Calculate percentage based on original Celsius values (for consistent bar behavior)
  const percentage = Math.min(100, (value / (setpoint || max)) * 100);
  
  const barColors = {
    default: 'bg-gradient-to-r from-amber-400 to-accent',
    steam: 'bg-gradient-to-r from-blue-400 to-violet-500',
    pressure: 'bg-gradient-to-r from-emerald-400 to-cyan-500',
  };

  return (
    <div className="card">
      <div className="flex items-center gap-2 mb-4">
        {icon && <span className="text-accent">{icon}</span>}
        <span className="font-semibold text-theme">{label}</span>
      </div>
      
      <div className="flex items-baseline gap-1 mb-3">
        <span className="text-4xl font-bold text-accent tabular-nums">
          {displayValue.toFixed(1)}
        </span>
        <span className="text-xl text-theme-muted">{displayUnit}</span>
      </div>
      
      <div className="relative h-3 bg-theme-secondary rounded-full overflow-hidden">
        <div
          className={cn(
            'absolute inset-y-0 left-0 rounded-full transition-all duration-500',
            barColors[variant]
          )}
          style={{ width: `${percentage}%` }}
        />
        {setpoint && showSetpoint && (
          <div
            className="absolute top-0 bottom-0 w-0.5 bg-theme"
            style={{ left: `${(setpoint / max) * 100}%` }}
          />
        )}
      </div>
      
      {displaySetpoint && showSetpoint && (
        <div className="mt-2 text-sm text-theme-muted">
          Setpoint: <span className="font-semibold text-theme-secondary">{displaySetpoint.toFixed(1)}{displayUnit}</span>
        </div>
      )}
    </div>
  );
});
