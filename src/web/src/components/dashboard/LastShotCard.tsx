import { memo, useMemo } from 'react';
import { useStore } from '@/lib/store';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Badge } from '@/components/Badge';
import { Coffee, TrendingUp, Award, Clock } from 'lucide-react';

/**
 * Last Shot Card - Shows information about the last shot and session stats.
 * Displayed on Dashboard when idle (not brewing).
 * Replaces the Pressure card which shows 0 when not brewing.
 * 
 * E2E Data Sources (all verified):
 * - shotsToday: ✅ ESP32 stats.daily.shotCount via status message
 * - sessionShots: ✅ ESP32 stats.sessionShots via status message
 * - totalShots: ✅ ESP32 stats.lifetime.totalShots, persisted in NVS
 * - avgBrewTimeMs: ✅ ESP32 stats.daily.avgBrewTimeMs via status message
 * - lastShotTimestamp: ✅ ESP32 Unix timestamp (ms) via machine.lastShotTimestamp
 */
export const LastShotCard = memo(function LastShotCard() {
  const shotsToday = useStore((s) => s.stats.shotsToday);
  const sessionShots = useStore((s) => s.stats.sessionShots);
  const daily = useStore((s) => s.stats.daily);
  const lifetime = useStore((s) => s.stats.lifetime);
  const lastShotTimestamp = useStore((s) => s.machine.lastShotTimestamp);
  const machineMode = useStore((s) => s.machine.mode);

  // Calculate time since last shot
  const timeSinceLastShot = useMemo(() => {
    if (!lastShotTimestamp) return null;
    
    const now = Date.now();
    const diff = now - lastShotTimestamp;
    
    // Sanity check - if timestamp is in the future or too old (> 30 days), skip
    if (diff < 0 || diff > 30 * 24 * 60 * 60 * 1000) return null;
    
    const minutes = Math.floor(diff / 60000);
    const hours = Math.floor(diff / 3600000);
    const days = Math.floor(diff / 86400000);
    
    if (minutes < 1) return 'just now';
    if (minutes < 60) return `${minutes}m ago`;
    if (hours < 24) return `${hours}h ago`;
    if (days === 1) return '1 day ago';
    return `${days} days ago`;
  }, [lastShotTimestamp]);

  // Get average brew time (prefer daily, fallback to lifetime)
  const avgBrewTimeSec = useMemo(() => {
    if (daily.avgBrewTimeMs > 0) {
      return (daily.avgBrewTimeMs / 1000).toFixed(1);
    }
    if (lifetime.avgBrewTimeMs > 0) {
      return (lifetime.avgBrewTimeMs / 1000).toFixed(1);
    }
    return null;
  }, [daily.avgBrewTimeMs, lifetime.avgBrewTimeMs]);

  // No shots ever
  if (lifetime.totalShots === 0) {
    return (
      <Card>
        <CardHeader>
          <CardTitle icon={<Coffee className="w-5 h-5" />}>Ready to Brew</CardTitle>
        </CardHeader>
        <div className="text-center py-6">
          <div className="w-16 h-16 bg-accent/10 rounded-full flex items-center justify-center mx-auto mb-4">
            <Coffee className="w-8 h-8 text-accent" />
          </div>
          <h3 className="text-lg font-semibold text-theme mb-2">No shots yet</h3>
          <p className="text-sm text-theme-muted">
            {machineMode === 'standby'
              ? 'Turn on your machine to start brewing'
              : 'Your first espresso awaits!'}
          </p>
        </div>
      </Card>
    );
  }

  return (
    <Card>
      <CardHeader
        action={
          timeSinceLastShot && (
            <Badge variant="info" className="text-xs">
              <Clock className="w-3 h-3 mr-1" />
              {timeSinceLastShot}
            </Badge>
          )
        }
      >
        <CardTitle icon={<Coffee className="w-5 h-5" />}>Shot Stats</CardTitle>
      </CardHeader>

      {/* Stats Grid */}
      <div className="grid grid-cols-2 gap-3">
        <StatItem
          icon={<Coffee className="w-4 h-4" />}
          label="Today"
          value={shotsToday.toString()}
          subtext={shotsToday === 1 ? 'shot' : 'shots'}
          highlight={shotsToday > 0}
        />
        <StatItem
          icon={<TrendingUp className="w-4 h-4" />}
          label="Session"
          value={sessionShots.toString()}
          subtext={sessionShots === 1 ? 'shot' : 'shots'}
        />
        {avgBrewTimeSec && (
          <StatItem
            icon={<Clock className="w-4 h-4" />}
            label="Avg Time"
            value={avgBrewTimeSec}
            subtext="seconds"
          />
        )}
        <StatItem
          icon={<Award className="w-4 h-4" />}
          label="Lifetime"
          value={formatNumber(lifetime.totalShots)}
          subtext="total shots"
        />
      </div>
    </Card>
  );
});

interface StatItemProps {
  icon: React.ReactNode;
  label: string;
  value: string;
  subtext?: string;
  highlight?: boolean;
}

function StatItem({ icon, label, value, subtext, highlight }: StatItemProps) {
  return (
    <div className="p-3 rounded-xl bg-theme-secondary">
      <div className="flex items-center gap-1.5 text-theme-muted mb-1">
        {icon}
        <span className="text-xs font-medium">{label}</span>
      </div>
      <div className={`text-2xl font-bold tabular-nums ${highlight ? 'text-accent' : 'text-theme'}`}>
        {value}
      </div>
      {subtext && <div className="text-xs text-theme-muted">{subtext}</div>}
    </div>
  );
}

function formatNumber(num: number): string {
  if (num >= 1000) {
    return (num / 1000).toFixed(1) + 'k';
  }
  return num.toString();
}

// Export a utility hook for checking if we should show this card
export function useShowLastShotCard() {
  const isBrewing = useStore((s) => s.machine.isBrewing);
  const shotActive = useStore((s) => s.shot.active);
  return !isBrewing && !shotActive;
}

