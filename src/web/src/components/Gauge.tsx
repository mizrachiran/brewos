import { cn } from '@/lib/utils';

interface GaugeProps {
  value: number;
  max: number;
  setpoint?: number;
  label: string;
  unit: string;
  icon?: React.ReactNode;
  variant?: 'default' | 'steam' | 'pressure';
  showSetpoint?: boolean;
}

export function Gauge({
  value,
  max,
  setpoint,
  label,
  unit,
  icon,
  variant = 'default',
  showSetpoint = true,
}: GaugeProps) {
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
          {value.toFixed(1)}
        </span>
        <span className="text-xl text-theme-muted">{unit}</span>
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
      
      {setpoint && showSetpoint && (
        <div className="mt-2 text-sm text-theme-muted">
          Setpoint: <span className="font-semibold text-theme-secondary">{setpoint.toFixed(1)}{unit}</span>
        </div>
      )}
    </div>
  );
}
