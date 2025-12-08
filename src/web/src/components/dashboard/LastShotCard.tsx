import { memo, useMemo } from 'react';
import { useStore } from '@/lib/store';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Badge } from '@/components/Badge';
import { Coffee, Clock, TrendingUp, History, Award, Target } from 'lucide-react';

/**
 * Last Shot Card - Shows information about the last shot and session stats.
 * Displayed on Dashboard when idle (not brewing).
 * Replaces the Pressure card which shows 0 when not brewing.
 */
export const LastShotCard = memo(function LastShotCard() {
  const lastShotTimestamp = useStore((s) => s.machine.lastShotTimestamp);
  const shotsToday = useStore((s) => s.stats.shotsToday);
  const sessionShots = useStore((s) => s.stats.sessionShots);
  const daily = useStore((s) => s.stats.daily);
  const lifetime = useStore((s) => s.stats.lifetime);
  const machineMode = useStore((s) => s.machine.mode);

  const timeSinceLastShot = useMemo(() => {
    if (!lastShotTimestamp) return null;
    // Calculate time distance
    const now = Date.now();
    const diff = now - lastShotTimestamp;
    const minutes = Math.floor(diff / 60000);
    const hours = Math.floor(diff / 3600000);
    const days = Math.floor(diff / 86400000);
    
    if (minutes < 1) return 'just now';
    if (minutes < 60) return `${minutes}m`;
    if (hours < 24) return `${hours}h`;
    if (days === 1) return '1 day';
    return `${days} days`;
  }, [lastShotTimestamp]);

  const averageBrewTime = useMemo(() => {
    if (daily.avgBrewTimeMs > 0) {
      return (daily.avgBrewTimeMs / 1000).toFixed(1);
    }
    if (lifetime.avgBrewTimeMs > 0) {
      return (lifetime.avgBrewTimeMs / 1000).toFixed(1);
    }
    return null;
  }, [daily.avgBrewTimeMs, lifetime.avgBrewTimeMs]);

  // No shots ever
  if (!lastShotTimestamp && lifetime.totalShots === 0) {
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
          lastShotTimestamp && (
            <Badge variant="info" className="text-xs">
              <Clock className="w-3 h-3 mr-1" />
              {timeSinceLastShot} ago
            </Badge>
          )
        }
      >
        <CardTitle icon={<Coffee className="w-5 h-5" />}>Shot Stats</CardTitle>
      </CardHeader>

      {/* Stats Grid */}
      <div className="grid grid-cols-2 gap-4 mb-4">
        <StatItem
          icon={<Coffee className="w-4 h-4" />}
          label="Today"
          value={shotsToday.toString()}
          subtext={shotsToday === 1 ? 'shot' : 'shots'}
          highlight={shotsToday > 0}
        />
        <StatItem
          icon={<TrendingUp className="w-4 h-4" />}
          label="This Session"
          value={sessionShots.toString()}
          subtext={sessionShots === 1 ? 'shot' : 'shots'}
        />
        {averageBrewTime && (
          <StatItem
            icon={<Clock className="w-4 h-4" />}
            label="Avg Time"
            value={averageBrewTime}
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

      {/* Quick insight or tip */}
      <QuickInsight shotsToday={shotsToday} daily={daily} />
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

interface QuickInsightProps {
  shotsToday: number;
  daily: { avgBrewTimeMs: number; shotCount: number };
}

function QuickInsight({ shotsToday, daily }: QuickInsightProps) {
  // Generate a contextual insight
  const getInsight = () => {
    const hour = new Date().getHours();

    if (shotsToday === 0) {
      if (hour < 12) {
        return { icon: Coffee, text: 'Good morning! Ready for your first cup?', type: 'greeting' };
      } else if (hour < 17) {
        return { icon: Coffee, text: 'Afternoon pick-me-up time?', type: 'greeting' };
      } else {
        return { icon: Coffee, text: 'Evening espresso? Why not!', type: 'greeting' };
      }
    }

    if (shotsToday >= 5) {
      return { icon: Award, text: 'Productive day! 5+ shots pulled', type: 'achievement' };
    }

    if (daily.avgBrewTimeMs > 0) {
      const avgSeconds = daily.avgBrewTimeMs / 1000;
      if (avgSeconds >= 25 && avgSeconds <= 35) {
        return { icon: Target, text: 'Great extraction times today!', type: 'success' };
      }
    }

    return { icon: History, text: 'View your shot history for detailed stats', type: 'info' };
  };

  const insight = getInsight();
  const Icon = insight.icon;

  const bgClass = {
    greeting: 'bg-amber-500/10 text-amber-700 dark:text-amber-300',
    achievement: 'bg-emerald-500/10 text-emerald-700 dark:text-emerald-300',
    success: 'bg-emerald-500/10 text-emerald-700 dark:text-emerald-300',
    info: 'bg-theme-secondary text-theme-muted',
  }[insight.type];

  return (
    <div className={`flex items-center gap-2 p-3 rounded-xl ${bgClass}`}>
      <Icon className="w-4 h-4 flex-shrink-0" />
      <span className="text-sm">{insight.text}</span>
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

