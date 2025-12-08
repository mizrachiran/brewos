import { ReactNode } from 'react';
import { useStore } from '@/lib/store';
import { cn } from '@/lib/utils';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Badge } from '@/components/Badge';
import { Button } from '@/components/Button';
import { Scale, Droplet, Timer } from 'lucide-react';

export interface ScaleStatusCardProps {
  /** Additional actions to show when connected */
  actions?: ReactNode;
  /** Content to show when disconnected */
  disconnectedContent?: ReactNode;
  /** Whether to show a compact version (smaller weight/flow display) */
  compact?: boolean;
  /** Custom class name for the card */
  className?: string;
}

/**
 * Reusable Scale Status Card component
 * 
 * Displays scale connection status, weight, flow rate, battery, and stability.
 * Used in both Brewing page and Settings/Scale page.
 */
export function ScaleStatusCard({
  actions,
  disconnectedContent,
  compact = false,
  className = '',
}: ScaleStatusCardProps) {
  const scale = useStore((s) => s.scale);

  return (
    <Card
      className={cn(
        scale.connected && 'bg-gradient-to-br from-emerald-500/5 to-emerald-500/0 border-emerald-500/20',
        className
      )}
    >
      <CardHeader
        action={
          <Badge variant={scale.connected ? 'success' : 'error'}>
            {scale.connected ? (
              <span className="flex items-center gap-1.5">
                <span className="relative flex h-2 w-2">
                  <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                  <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-500"></span>
                </span>
                Connected
              </span>
            ) : (
              'Not connected'
            )}
          </Badge>
        }
      >
        <CardTitle icon={<Scale className="w-5 h-5" />}>Scale</CardTitle>
      </CardHeader>

      {scale.connected ? (
        <>
          {/* Scale Info Bar */}
          <div className="flex flex-wrap items-center gap-3 mb-4 pb-4 border-b border-theme">
            <div className="flex items-center gap-2">
              <span className="text-sm font-medium text-theme">
                {scale.name || 'BLE Scale'}
              </span>
              {scale.type && (
                <Badge variant="info" className="text-xs">
                  {scale.type}
                </Badge>
              )}
            </div>
            <div className="flex items-center gap-3 ml-auto">
              {scale.battery > 0 && (
                <div className="flex items-center gap-1 text-sm text-theme-muted">
                  <BatteryIcon level={scale.battery} />
                  <span>{scale.battery}%</span>
                </div>
              )}
              <Badge
                variant={scale.stable ? 'success' : 'warning'}
                className="text-xs"
              >
                {scale.stable ? '● Stable' : '◐ Settling'}
              </Badge>
            </div>
          </div>

          {/* Weight & Flow */}
          <div className={`grid grid-cols-2 gap-4 ${actions ? 'mb-6' : ''}`}>
            <WeightDisplay 
              value={scale.weight} 
              compact={compact} 
            />
            <FlowRateDisplay 
              value={scale.flowRate} 
              compact={compact} 
            />
          </div>

          {/* Custom Actions */}
          {actions}
        </>
      ) : (
        disconnectedContent || <DefaultDisconnectedContent />
      )}
    </Card>
  );
}

/**
 * Battery icon with dynamic fill level
 */
function BatteryIcon({ level }: { level: number }) {
  return (
    <svg
      className="w-4 h-4"
      fill="none"
      viewBox="0 0 24 24"
      stroke="currentColor"
    >
      <path
        strokeLinecap="round"
        strokeLinejoin="round"
        strokeWidth={2}
        d="M3 7h14a2 2 0 012 2v6a2 2 0 01-2 2H3a2 2 0 01-2-2V9a2 2 0 012-2zm18 3v4"
      />
      <rect
        x="4"
        y="9"
        width={Math.max(2, (level / 100) * 10)}
        height="6"
        rx="1"
        fill="currentColor"
      />
    </svg>
  );
}

/**
 * Weight display component with optional compact mode
 */
function WeightDisplay({ value, compact }: { value: number; compact?: boolean }) {
  if (compact) {
    return (
      <div className="p-3 bg-emerald-500/10 rounded-xl border border-emerald-500/20">
        <div className="text-xs text-emerald-700 dark:text-emerald-300/70 mb-1">Weight</div>
        <div className="text-xl font-bold text-theme tabular-nums">
          {value.toFixed(1)} <span className="text-sm font-normal text-theme-muted">g</span>
        </div>
      </div>
    );
  }

  return (
    <div className="text-center p-4 bg-emerald-500/10 rounded-xl border border-emerald-500/20">
      <Droplet className="w-6 h-6 text-emerald-600 dark:text-emerald-400 mx-auto mb-2" />
      <div className="text-3xl font-bold text-theme tabular-nums">
        {value.toFixed(1)}
      </div>
      <div className="text-sm text-theme-muted">grams</div>
    </div>
  );
}

/**
 * Flow rate display component with optional compact mode
 */
function FlowRateDisplay({ value, compact }: { value: number; compact?: boolean }) {
  if (compact) {
    return (
      <div className="p-3 bg-emerald-500/10 rounded-xl border border-emerald-500/20">
        <div className="text-xs text-emerald-700 dark:text-emerald-300/70 mb-1">Flow Rate</div>
        <div className="text-xl font-bold text-theme tabular-nums">
          {value.toFixed(1)} <span className="text-sm font-normal text-theme-muted">g/s</span>
        </div>
      </div>
    );
  }

  return (
    <div className="text-center p-4 bg-emerald-500/10 rounded-xl border border-emerald-500/20">
      <Timer className="w-6 h-6 text-emerald-600 dark:text-emerald-400 mx-auto mb-2" />
      <div className="text-3xl font-bold text-theme tabular-nums">
        {value.toFixed(1)}
      </div>
      <div className="text-sm text-theme-muted">g/s flow</div>
    </div>
  );
}

/**
 * Default content shown when no scale is connected
 */
function DefaultDisconnectedContent() {
  return (
    <div className="text-center py-8">
      <div className="w-16 h-16 bg-theme-secondary rounded-full flex items-center justify-center mx-auto mb-4">
        <Scale className="w-8 h-8 text-theme-muted" />
      </div>
      <h3 className="text-lg font-semibold text-theme mb-1">No Scale Connected</h3>
      <p className="text-sm text-theme-muted mb-4">
        Connect a Bluetooth scale to enable brew-by-weight
      </p>
      <Button
        variant="secondary"
        onClick={() => (window.location.href = '/settings#scale')}
      >
        Connect Scale
      </Button>
    </div>
  );
}

/**
 * Export sub-components for custom compositions
 */
export { BatteryIcon, WeightDisplay, FlowRateDisplay, DefaultDisconnectedContent };

