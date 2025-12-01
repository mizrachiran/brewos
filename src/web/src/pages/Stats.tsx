import { useState, useEffect } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Button } from '@/components/Button';
import { Badge } from '@/components/Badge';
import { PageHeader } from '@/components/PageHeader';
import { formatDate } from '@/lib/date';
import { 
  BarChart3, 
  Coffee, 
  Clock, 
  Zap,
  TrendingUp,
  Calendar,
  Sparkles,
  RefreshCw,
  Target,
  Timer,
  Flame,
  Droplets,
} from 'lucide-react';

interface WeeklyData {
  day: string;
  shots: number;
}

export function Stats() {
  const stats = useStore((s) => s.stats);
  const [weeklyData, setWeeklyData] = useState<WeeklyData[]>([]);
  const [loading, setLoading] = useState(false);

  // Fetch extended stats from API
  useEffect(() => {
    fetchExtendedStats();
  }, []);

  const fetchExtendedStats = async () => {
    setLoading(true);
    try {
      const response = await fetch('/api/stats');
      if (response.ok) {
        const data = await response.json();
        if (data.weekly) {
          setWeeklyData(data.weekly);
        }
      }
    } catch {
      // Generate estimated weekly data from available stats
      generateWeeklyEstimate();
    }
    setLoading(false);
  };

  const generateWeeklyEstimate = () => {
    // Create placeholder weekly data based on weeklyCount
    const days = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const avgPerDay = stats.weeklyCount > 0 ? Math.round(stats.weeklyCount / 7) : 0;
    const weekly = days.map((day, index) => {
      // Today's day gets shotsToday, others get average estimate
      const today = new Date().getDay();
      const dayIndex = (index + 1) % 7; // Convert Mon=0 to Sun=0 format
      return {
        day,
        shots: dayIndex === today ? stats.shotsToday : avgPerDay,
      };
    });
    setWeeklyData(weekly);
  };

  // Update weekly estimate when stats change
  useEffect(() => {
    if (weeklyData.length === 0 || stats.weeklyCount > 0) {
      generateWeeklyEstimate();
    }
  }, [stats.weeklyCount, stats.shotsToday]);

  const markCleaning = (type: 'backflush' | 'groupClean' | 'descale') => {
    getConnection()?.sendCommand('record_maintenance', { type });
  };

  // Calculate average shots per day
  const avgShotsPerDay = stats.totalOnTimeMinutes > 0 && stats.totalShots > 0
    ? (stats.totalShots / Math.max(1, Math.ceil(stats.totalOnTimeMinutes / 1440))).toFixed(1)
    : stats.dailyCount > 0 ? stats.dailyCount.toString() : '0';

  const maxShots = weeklyData.length > 0 
    ? Math.max(...weeklyData.map(d => d.shots), 1) 
    : 5;

  // Format avg shot time from ms to seconds
  const avgShotTimeSec = stats.avgBrewTimeMs > 0 
    ? (stats.avgBrewTimeMs / 1000).toFixed(1) 
    : null;

  return (
    <div className="space-y-6">
      {/* Header */}
      <PageHeader
        title="Statistics"
        subtitle="Track your brewing journey"
        action={
          <Button variant="secondary" onClick={fetchExtendedStats} disabled={loading}>
            <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
            Refresh
          </Button>
        }
      />

      {/* Key Metrics */}
      <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
        <MetricCard
          icon={<Coffee className="w-5 h-5" />}
          label="Total Shots"
          value={stats.totalShots.toLocaleString()}
          color="accent"
        />
        <MetricCard
          icon={<Calendar className="w-5 h-5" />}
          label="Today"
          value={stats.shotsToday}
          subtext={`Avg: ${avgShotsPerDay}/day`}
          color="emerald"
        />
        <MetricCard
          icon={<Sparkles className="w-5 h-5" />}
          label="Since Backflush"
          value={stats.shotsSinceBackflush}
          warning={stats.shotsSinceBackflush > 100}
          subtext={stats.shotsSinceBackflush > 100 ? 'Time to clean!' : 'Looking good'}
          color={stats.shotsSinceBackflush > 100 ? 'amber' : 'blue'}
        />
        <MetricCard
          icon={<Timer className="w-5 h-5" />}
          label="Avg Shot Time"
          value={avgShotTimeSec ? `${avgShotTimeSec}s` : '—'}
          subtext={stats.minBrewTimeMs > 0 ? `${(stats.minBrewTimeMs/1000).toFixed(0)}-${(stats.maxBrewTimeMs/1000).toFixed(0)}s range` : undefined}
          color="purple"
        />
      </div>

      {/* Weekly Chart */}
      <Card>
        <CardHeader>
          <CardTitle icon={<BarChart3 className="w-5 h-5" />}>
            This Week ({stats.weeklyCount} shots)
          </CardTitle>
        </CardHeader>

        <div className="h-48">
          {weeklyData.length > 0 ? (
            <div className="flex items-end justify-between h-full gap-2 pt-4">
              {weeklyData.map((data) => (
                <div key={data.day} className="flex-1 flex flex-col items-center">
                  <div className="w-full flex flex-col items-center justify-end h-32">
                    <span className="text-xs font-semibold text-theme-secondary mb-1">
                      {data.shots > 0 ? data.shots : ''}
                    </span>
                    <div 
                      className="w-full max-w-12 bg-gradient-to-t from-accent to-accent-light rounded-t-lg transition-all duration-500"
                      style={{ 
                        height: `${Math.max((data.shots / maxShots) * 100, 4)}%`,
                        opacity: data.shots > 0 ? 1 : 0.2,
                      }}
                    />
                  </div>
                  <span className="text-xs text-theme-muted mt-2 font-medium">{data.day}</span>
                </div>
              ))}
            </div>
          ) : (
            <div className="flex items-center justify-center h-full text-theme-muted">
              <p>No data available yet. Start brewing!</p>
            </div>
          )}
        </div>
      </Card>

      {/* Detailed Stats Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* Brewing Stats */}
        <Card>
          <CardHeader>
            <CardTitle icon={<Target className="w-5 h-5" />}>
              Brewing Stats
            </CardTitle>
          </CardHeader>

          <div className="space-y-4">
            <StatRow 
              label="Average Shot Time" 
              value={avgShotTimeSec ? `${avgShotTimeSec}s` : '—'} 
              icon={<Timer className="w-4 h-4" />}
            />
            <StatRow 
              label="Fastest Shot" 
              value={stats.minBrewTimeMs > 0 ? `${(stats.minBrewTimeMs / 1000).toFixed(1)}s` : '—'} 
              icon={<TrendingUp className="w-4 h-4" />}
            />
            <StatRow 
              label="Longest Shot" 
              value={stats.maxBrewTimeMs > 0 ? `${(stats.maxBrewTimeMs / 1000).toFixed(1)}s` : '—'} 
              icon={<Clock className="w-4 h-4" />}
            />
            <StatRow 
              label="This Month" 
              value={`${stats.monthlyCount} shots`} 
              icon={<Calendar className="w-4 h-4" />}
            />
          </div>
        </Card>

        {/* Machine Stats */}
        <Card>
          <CardHeader>
            <CardTitle icon={<Flame className="w-5 h-5" />}>
              Machine Usage
            </CardTitle>
          </CardHeader>

          <div className="space-y-4">
            <StatRow 
              label="Total Runtime" 
              value={formatRuntime(stats.totalOnTimeMinutes * 60)} 
              icon={<Clock className="w-4 h-4" />}
            />
            <StatRow 
              label="Session Shots" 
              value={stats.sessionShots.toString()} 
              icon={<Coffee className="w-4 h-4" />}
            />
            <StatRow 
              label="Steam Cycles" 
              value={stats.totalSteamCycles.toLocaleString()} 
              icon={<Droplets className="w-4 h-4" />}
            />
            <StatRow 
              label="Energy Used" 
              value={stats.totalKwh > 0 ? `${stats.totalKwh.toFixed(1)} kWh` : '—'} 
              icon={<Zap className="w-4 h-4" />}
            />
          </div>
        </Card>
      </div>

      {/* Maintenance */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Sparkles className="w-5 h-5" />}>
            Maintenance
          </CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 mb-6">
          <MaintenanceCard
            label="Backflush"
            shotsSince={stats.shotsSinceBackflush}
            lastTimestamp={stats.lastBackflushTimestamp}
            threshold={100}
            onMark={() => markCleaning('backflush')}
          />
          <MaintenanceCard
            label="Group Clean"
            shotsSince={stats.shotsSinceGroupClean}
            lastTimestamp={stats.lastGroupCleanTimestamp}
            threshold={50}
            onMark={() => markCleaning('groupClean')}
          />
          <MaintenanceCard
            label="Descale"
            shotsSince={stats.shotsSinceDescale}
            lastTimestamp={stats.lastDescaleTimestamp}
            threshold={500}
            onMark={() => markCleaning('descale')}
          />
        </div>
      </Card>

      {/* Milestones */}
      {stats.totalShots > 0 && (
        <Card>
          <CardHeader>
            <CardTitle icon={<TrendingUp className="w-5 h-5" />}>
              Milestones
            </CardTitle>
          </CardHeader>

          <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
            <MilestoneCard 
              label="First Shot" 
              achieved={stats.totalShots >= 1}
              icon="☕"
            />
            <MilestoneCard 
              label="10 Shots" 
              achieved={stats.totalShots >= 10}
              icon="🎯"
            />
            <MilestoneCard 
              label="100 Shots" 
              achieved={stats.totalShots >= 100}
              icon="🏆"
            />
            <MilestoneCard 
              label="1000 Shots" 
              achieved={stats.totalShots >= 1000}
              icon="👑"
            />
          </div>
        </Card>
      )}
    </div>
  );
}

interface MetricCardProps {
  icon: React.ReactNode;
  label: string;
  value: string | number;
  subtext?: string;
  warning?: boolean;
  color?: 'accent' | 'emerald' | 'amber' | 'blue' | 'purple';
}

function MetricCard({ icon, label, value, subtext, warning, color = 'accent' }: MetricCardProps) {
  const iconColorClasses = {
    accent: 'text-accent',
    emerald: 'text-emerald-500',
    amber: 'text-amber-500',
    blue: 'text-blue-500',
    purple: 'text-purple-500',
  };

  return (
    <Card className="flex flex-col">
      <div className="flex items-center gap-2 mb-3">
        <span className={iconColorClasses[color]}>{icon}</span>
        <span className="text-sm font-medium text-theme-muted">{label}</span>
        {warning && <Badge variant="warning" className="ml-auto">!</Badge>}
      </div>
      <div className={`text-3xl font-bold ${warning ? 'text-amber-500' : 'text-theme'}`}>
        {value}
      </div>
      {subtext && (
        <div className="text-xs text-theme-muted mt-1">{subtext}</div>
      )}
    </Card>
  );
}

interface StatRowProps {
  label: string;
  value: string;
  icon: React.ReactNode;
}

function StatRow({ label, value, icon }: StatRowProps) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-theme last:border-0">
      <div className="flex items-center gap-2 text-theme-muted">
        {icon}
        <span className="text-sm">{label}</span>
      </div>
      <span className="text-sm font-semibold text-theme">{value}</span>
    </div>
  );
}

interface MaintenanceCardProps {
  label: string;
  shotsSince: number;
  lastTimestamp: number;
  threshold: number;
  onMark: () => void;
}

function MaintenanceCard({ label, shotsSince, lastTimestamp, threshold, onMark }: MaintenanceCardProps) {
  const isOverdue = shotsSince >= threshold;
  const lastDate = lastTimestamp > 0 
    ? formatDate(lastTimestamp, { dateStyle: 'short' })
    : 'Never';

  return (
    <div className={`p-4 rounded-xl ${isOverdue ? 'bg-amber-500/10 border border-amber-500/30' : 'bg-theme-secondary'}`}>
      <div className="flex items-center justify-between mb-2">
        <span className="text-sm font-medium text-theme-secondary">{label}</span>
        {isOverdue && <Badge variant="warning">Due</Badge>}
      </div>
      <div className={`text-2xl font-bold ${isOverdue ? 'text-amber-500' : 'text-theme'}`}>
        {shotsSince}
      </div>
      <div className="text-xs text-theme-muted mb-3">shots since last</div>
      <div className="flex items-center justify-between">
        <span className="text-xs text-theme-muted">Last: {lastDate}</span>
        <Button size="sm" variant={isOverdue ? 'primary' : 'secondary'} onClick={onMark}>
          Mark Done
        </Button>
      </div>
    </div>
  );
}

interface MilestoneCardProps {
  label: string;
  achieved: boolean;
  icon: string;
}

function MilestoneCard({ label, achieved, icon }: MilestoneCardProps) {
  return (
    <div className={`p-4 rounded-xl text-center transition-all ${
      achieved 
        ? 'bg-gradient-to-br from-accent/10 to-accent/5 border border-accent/20' 
        : 'bg-theme-secondary opacity-50'
    }`}>
      <div className="text-2xl mb-2">{icon}</div>
      <div className={`text-sm font-medium ${achieved ? 'text-theme' : 'text-theme-muted'}`}>
        {label}
      </div>
      {achieved && (
        <Badge variant="success" className="mt-2">Achieved</Badge>
      )}
    </div>
  );
}

function formatRuntime(seconds: number): string {
  if (seconds <= 0) return '—';
  if (seconds < 3600) {
    return `${Math.floor(seconds / 60)}m`;
  } else if (seconds < 86400) {
    return `${Math.floor(seconds / 3600)}h ${Math.floor((seconds % 3600) / 60)}m`;
  } else {
    return `${Math.floor(seconds / 86400)}d ${Math.floor((seconds % 86400) / 3600)}h`;
  }
}
