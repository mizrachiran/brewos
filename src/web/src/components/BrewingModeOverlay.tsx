import { useState, useEffect, useCallback, memo } from 'react';
import { useStore } from '@/lib/store';
import { cn } from '@/lib/utils';
import { Badge } from '@/components/Badge';
import {
  Minimize2,
  Maximize2,
  Droplet,
  Waves,
  Gauge,
  Thermometer,
  Coffee,
  Timer,
} from 'lucide-react';

const MAX_PRESSURE = 12; // bar - typical max for espresso

interface DataPoint {
  time: number;
  pressure: number;
}

/**
 * Full-screen brewing overlay that appears during shot extraction.
 * Shows real-time shot timer, weight, flow rate, pressure, and progress.
 */
export const BrewingModeOverlay = memo(function BrewingModeOverlay() {
  const shot = useStore((s) => s.shot);
  const pressure = useStore((s) => s.pressure);
  const bbw = useStore((s) => s.bbw);
  const preinfusion = useStore((s) => s.preinfusion);
  const brewTemp = useStore((s) => s.temps.brew.current);
  const brewSetpoint = useStore((s) => s.temps.brew.setpoint);
  const hasPressureSensor = useStore((s) => s.device.hasPressureSensor);
  const scaleConnected = useStore((s) => s.scale.connected);

  const [minimized, setMinimized] = useState(false);
  const [shotTime, setShotTime] = useState(0);
  const [pressureData, setPressureData] = useState<DataPoint[]>([]);
  const [peakPressure, setPeakPressure] = useState(0);

  // Shot timer
  useEffect(() => {
    if (!shot.active) {
      setShotTime(0);
      setPressureData([]);
      setPeakPressure(0);
      return;
    }

    const interval = setInterval(() => {
      const elapsed = Date.now() - shot.startTime;
      setShotTime(elapsed);
    }, 100);

    return () => clearInterval(interval);
  }, [shot.active, shot.startTime]);

  // Record pressure data during brewing
  useEffect(() => {
    if (!shot.active) return;

    const elapsed = (Date.now() - shot.startTime) / 1000;
    setPressureData((prev) => {
      const last = prev[prev.length - 1];
      // Only add if enough time has passed or pressure changed significantly
      if (!last || elapsed - last.time > 0.2 || Math.abs(pressure - last.pressure) > 0.2) {
        return [...prev.slice(-200), { time: elapsed, pressure }]; // Keep last 200 points
      }
      return prev;
    });

    // Track peak pressure
    if (pressure > peakPressure) {
      setPeakPressure(pressure);
    }
  }, [shot.active, shot.startTime, pressure, peakPressure]);

  // Determine current phase
  const getPhase = useCallback(() => {
    if (!shot.active) return 'idle';
    if (!preinfusion.enabled) return 'extraction';

    const elapsed = shotTime;
    if (elapsed < preinfusion.onTimeMs) return 'wetting';
    if (elapsed < preinfusion.onTimeMs + preinfusion.pauseTimeMs) return 'soaking';
    return 'extraction';
  }, [shot.active, shotTime, preinfusion]);

  const phase = getPhase();

  // Don't render if not brewing
  if (!shot.active) return null;

  // Format time as MM:SS.d
  const formatTime = (ms: number) => {
    const totalSeconds = ms / 1000;
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${minutes.toString().padStart(2, '0')}:${seconds.toFixed(1).padStart(4, '0')}`;
  };

  // Progress toward target
  const progress = bbw.targetWeight > 0 ? Math.min(100, (shot.weight / bbw.targetWeight) * 100) : 0;

  // Minimized widget view
  if (minimized) {
    return (
      <div
        className="fixed bottom-4 right-4 z-50 bg-theme/95 backdrop-blur-xl rounded-2xl border border-accent/50 shadow-2xl p-4 cursor-pointer hover:scale-105 transition-transform"
        onClick={() => setMinimized(false)}
      >
        <div className="flex items-center gap-4">
          <Coffee className="w-6 h-6 text-accent animate-pulse" />
          <div>
            <div className="text-2xl font-mono font-bold text-theme tabular-nums">
              {formatTime(shotTime)}
            </div>
            <div className="text-sm text-theme-muted">
              {scaleConnected && `${shot.weight.toFixed(1)}g`}
              {scaleConnected && hasPressureSensor && ' • '}
              {hasPressureSensor && `${pressure.toFixed(1)} bar`}
              {!scaleConnected && !hasPressureSensor && 'Time-based'}
            </div>
          </div>
          <Maximize2 className="w-5 h-5 text-theme-muted" />
        </div>
      </div>
    );
  }

  return (
    <div className="fixed inset-0 z-50 bg-theme/98 backdrop-blur-md flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between p-3 sm:p-4 border-b border-theme flex-shrink-0">
        <div className="flex items-center gap-2 sm:gap-3 min-w-0">
          <Coffee className="w-5 h-5 sm:w-6 sm:h-6 text-accent flex-shrink-0" />
          <span className="hidden sm:inline text-lg font-semibold text-theme">Brewing</span>
          <PhaseIndicator phase={phase} preinfusion={preinfusion} />
        </div>
        <button
          onClick={() => setMinimized(true)}
          className="p-2 rounded-lg hover:bg-theme-secondary transition-colors flex-shrink-0"
          title="Minimize to corner"
        >
          <Minimize2 className="w-5 h-5 text-theme-muted" />
        </button>
      </div>

      {/* Main Content - Scrollable on small screens */}
      <div className="flex-1 overflow-y-auto">
        <div className="flex flex-col items-center p-4 sm:p-6 gap-4 sm:gap-6 min-h-full justify-center">
          {/* Shot Timer - Large and prominent */}
          <div className="text-center">
            <div className="text-5xl sm:text-7xl md:text-8xl font-mono font-bold text-theme tabular-nums tracking-tight">
              {formatTime(shotTime)}
            </div>
          </div>

        {/* Weight and Flow - only when scale connected */}
        {scaleConnected ? (
          <>
            <div className="grid grid-cols-2 gap-3 sm:gap-6 w-full max-w-lg">
              <MetricCard
                icon={<Droplet className="w-6 h-6 sm:w-8 sm:h-8" />}
                value={shot.weight.toFixed(1)}
                unit="g"
                label="Weight"
                color="emerald"
              />
              <MetricCard
                icon={<Waves className="w-6 h-6 sm:w-8 sm:h-8" />}
                value={shot.flowRate.toFixed(1)}
                unit="g/s"
                label="Flow Rate"
                color="cyan"
              />
            </div>

            {/* Progress Bar - only with scale and BBW enabled */}
            {bbw.enabled && bbw.targetWeight > 0 && (
              <div className="w-full max-w-lg bg-theme-secondary/80 rounded-xl p-3 sm:p-4 border border-theme">
                <div className="flex items-center justify-between mb-2">
                  <span className="text-sm text-theme-muted">Progress to target</span>
                  <span className="text-sm text-theme font-semibold">
                    {shot.weight.toFixed(1)}g / {bbw.targetWeight}g
                  </span>
                </div>
                <div className="h-3 sm:h-4 bg-theme-tertiary rounded-full overflow-hidden border border-theme">
                  <div
                    className="h-full bg-gradient-to-r from-amber-400 to-emerald-400 rounded-full transition-all duration-200"
                    style={{ width: `${progress}%` }}
                  />
                </div>
                <div className="flex justify-between mt-1.5 text-xs text-theme-muted">
                  <span>0g</span>
                  <span className="font-medium">{Math.round(progress)}%</span>
                  <span>{bbw.targetWeight}g</span>
                </div>
              </div>
            )}
          </>
        ) : (
          /* Time-based extraction indicator when no scale */
          <div className="w-full max-w-lg">
            <div className="bg-theme-secondary rounded-2xl p-4 sm:p-6 border border-theme text-center">
              <Timer className="w-8 h-8 sm:w-10 sm:h-10 text-amber-500 mx-auto mb-2 sm:mb-3" />
              <div className="text-base sm:text-lg font-medium text-theme">Time-Based Extraction</div>
              <div className="text-xs sm:text-sm text-theme-muted mt-1">
                Connect a scale for weight tracking
              </div>
            </div>
          </div>
        )}

        {/* Pressure and Temperature */}
        <div className={cn('grid gap-3 sm:gap-6 w-full max-w-lg', hasPressureSensor ? 'grid-cols-2' : 'grid-cols-1')}>
          {/* Pressure Card - only if sensor installed */}
          {hasPressureSensor && (
            <div className="bg-theme-secondary rounded-xl sm:rounded-2xl p-3 sm:p-4 border border-theme">
              <div className="flex items-center gap-2 mb-2 sm:mb-3">
                <Gauge className="w-4 h-4 sm:w-5 sm:h-5 text-cyan-500" />
                <span className="text-xs sm:text-sm text-theme-muted">Pressure</span>
              </div>
              <div className="text-2xl sm:text-3xl font-bold text-theme tabular-nums">
                {pressure.toFixed(1)} <span className="text-sm sm:text-lg text-theme-muted">bar</span>
              </div>
              <PressureBar pressure={pressure} />
              <div className="text-xs text-theme-muted mt-1 sm:mt-2">Peak: {peakPressure.toFixed(1)} bar</div>
            </div>
          )}

          {/* Temperature Card */}
          <div className="bg-theme-secondary rounded-xl sm:rounded-2xl p-3 sm:p-4 border border-theme">
            <div className="flex items-center gap-2 mb-2 sm:mb-3">
              <Thermometer className="w-4 h-4 sm:w-5 sm:h-5 text-orange-500" />
              <span className="text-xs sm:text-sm text-theme-muted">Temperature</span>
            </div>
            <div className="text-2xl sm:text-3xl font-bold text-theme tabular-nums">
              {brewTemp.toFixed(1)} <span className="text-sm sm:text-lg text-theme-muted">°C</span>
            </div>
            <div className="h-2 bg-theme-tertiary rounded-full overflow-hidden mt-2 sm:mt-3">
              <div
                className={cn(
                  'h-full rounded-full transition-all',
                  Math.abs(brewTemp - brewSetpoint) < 1 ? 'bg-emerald-400' : 'bg-orange-400'
                )}
                style={{ width: `${Math.min(100, (brewTemp / brewSetpoint) * 100)}%` }}
              />
            </div>
            <div className="text-xs text-theme-muted mt-1 sm:mt-2">Setpoint: {brewSetpoint}°C</div>
          </div>
        </div>

        {/* Pressure Graph - only if sensor installed, always visible to prevent layout jump */}
        {hasPressureSensor && (
          <div className="w-full max-w-lg">
            <PressureGraph data={pressureData} />
          </div>
        )}
        </div>
      </div>

      {/* Footer hint */}
      <div className="p-3 border-t border-theme flex items-center justify-center">
        <span className="text-xs text-theme-muted">Release lever to stop brew</span>
      </div>
    </div>
  );
});

interface MetricCardProps {
  icon: React.ReactNode;
  value: string;
  unit: string;
  label: string;
  color: 'emerald' | 'cyan' | 'orange' | 'amber';
}

function MetricCard({ icon, value, unit, label, color }: MetricCardProps) {
  const colorClasses = {
    emerald: 'text-emerald-500 bg-emerald-500/10 border-emerald-500/20',
    cyan: 'text-cyan-500 bg-cyan-500/10 border-cyan-500/20',
    orange: 'text-orange-500 bg-orange-500/10 border-orange-500/20',
    amber: 'text-amber-500 bg-amber-500/10 border-amber-500/20',
  };

  return (
    <div className={cn('rounded-xl sm:rounded-2xl p-4 sm:p-6 border', colorClasses[color])}>
      <div className={cn('mb-1 sm:mb-2', colorClasses[color].split(' ')[0])}>{icon}</div>
      <div className="text-3xl sm:text-4xl font-bold text-theme tabular-nums">
        {value}
        <span className="text-sm sm:text-lg text-theme-muted ml-1">{unit}</span>
      </div>
      <div className="text-xs sm:text-sm text-theme-muted mt-1">{label}</div>
    </div>
  );
}

function PressureBar({ pressure }: { pressure: number }) {
  const percentage = Math.min(100, (pressure / MAX_PRESSURE) * 100);
  const isOptimal = pressure >= 8 && pressure <= 10;

  return (
    <div className="h-2 bg-theme-tertiary rounded-full overflow-hidden mt-3 relative">
      {/* Optimal zone indicator */}
      <div
        className="absolute h-full bg-emerald-500/20"
        style={{ left: `${(8 / MAX_PRESSURE) * 100}%`, width: `${(2 / MAX_PRESSURE) * 100}%` }}
      />
      <div
        className={cn(
          'h-full rounded-full transition-all duration-150',
          isOptimal ? 'bg-emerald-400' : pressure < 8 ? 'bg-cyan-400' : 'bg-orange-400'
        )}
        style={{ width: `${percentage}%` }}
      />
    </div>
  );
}

interface PhaseIndicatorProps {
  phase: string;
  preinfusion: { enabled: boolean; onTimeMs: number; pauseTimeMs: number };
}

function PhaseIndicator({ phase, preinfusion }: PhaseIndicatorProps) {
  if (!preinfusion.enabled) {
    return (
      <Badge variant="success" className="bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 border-emerald-500/30 text-xs">
        Extracting
      </Badge>
    );
  }

  const phases = [
    { id: 'wetting', label: 'Wetting', shortLabel: 'Wet' },
    { id: 'soaking', label: 'Soaking', shortLabel: 'Soak' },
    { id: 'extraction', label: 'Extracting', shortLabel: 'Extract' },
  ];

  const currentPhase = phases.find((p) => p.id === phase);
  const currentIndex = phases.findIndex((p) => p.id === phase);

  return (
    <>
      {/* Mobile: Show only current phase */}
      <div className="sm:hidden">
        <Badge
          variant="success"
          className="bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 border-emerald-500/30 animate-pulse text-xs"
        >
          {currentPhase?.shortLabel || 'Brewing'}
        </Badge>
      </div>

      {/* Desktop: Show full flow */}
      <div className="hidden sm:flex items-center gap-1">
        {phases.map((p, i) => {
          const isActive = phase === p.id;
          const isPast = currentIndex > i;

          return (
            <div key={p.id} className="flex items-center gap-1">
              {i > 0 && <span className="text-theme-muted">→</span>}
              <Badge
                variant={isActive ? 'success' : isPast ? 'info' : 'warning'}
                className={cn(
                  'text-xs',
                  isActive && 'bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 border-emerald-500/30 animate-pulse',
                  isPast && 'bg-blue-500/20 text-blue-600 dark:text-blue-400 border-blue-500/30',
                  !isActive && !isPast && 'bg-theme-secondary text-theme-muted border-theme'
                )}
              >
                {p.label}
              </Badge>
            </div>
          );
        })}
      </div>
    </>
  );
}

function PressureGraph({ data }: { data: DataPoint[] }) {
  const width = 400;
  const height = 80;
  const padding = { left: 30, right: 10, top: 5, bottom: 20 };
  const graphWidth = width - padding.left - padding.right;
  const graphHeight = height - padding.top - padding.bottom;

  const maxTime = Math.max(30, data[data.length - 1]?.time || 0);
  const timeRange = Math.ceil(maxTime / 10) * 10;

  const xScale = (time: number) => padding.left + (time / timeRange) * graphWidth;
  const yScale = (p: number) => padding.top + graphHeight - (p / MAX_PRESSURE) * graphHeight;

  const pathD = data.reduce((path, point, i) => {
    const x = xScale(point.time);
    const y = yScale(point.pressure);
    return path + (i === 0 ? `M ${x} ${y}` : ` L ${x} ${y}`);
  }, '');

  return (
    <div className="bg-theme-secondary rounded-xl p-3 border border-theme">
      <div className="text-xs text-theme-muted mb-2">Pressure Profile</div>
      <svg width="100%" height={height} viewBox={`0 0 ${width} ${height}`} className="overflow-visible">
        {/* Optimal zone */}
        <rect
          x={padding.left}
          y={yScale(10)}
          width={graphWidth}
          height={yScale(8) - yScale(10)}
          fill="rgb(16, 185, 129)"
          opacity="0.1"
        />

        {/* Grid lines */}
        {[0, 4, 8, 12].map((p) => (
          <g key={p}>
            <line
              x1={padding.left}
              y1={yScale(p)}
              x2={width - padding.right}
              y2={yScale(p)}
              stroke="currentColor"
              strokeOpacity="0.15"
              strokeDasharray="2,2"
            />
            <text x={padding.left - 5} y={yScale(p)} textAnchor="end" className="text-[10px] fill-current opacity-40">
              {p}
            </text>
          </g>
        ))}

        {/* Pressure line */}
        <path d={pathD} fill="none" stroke="rgb(34, 211, 238)" strokeWidth="2" strokeLinecap="round" />

        {/* Current point */}
        {data.length > 0 && (
          <circle
            cx={xScale(data[data.length - 1].time)}
            cy={yScale(data[data.length - 1].pressure)}
            r="4"
            fill="rgb(34, 211, 238)"
          />
        )}
      </svg>
    </div>
  );
}

