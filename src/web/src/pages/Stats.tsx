import { useState, useEffect } from 'react';
import { useStore } from '@/lib/store';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Button } from '@/components/Button';
import { Badge } from '@/components/Badge';
import { 
  BarChart3, 
  Coffee, 
  Clock, 
  Zap,
  Droplets,
  TrendingUp,
  Calendar,
  Sparkles,
  RefreshCw,
  Target,
  Timer,
  Flame,
} from 'lucide-react';

interface DailyStats {
  date: string;
  shots: number;
  avgTime: number;
  avgWeight: number;
}

interface WeeklyData {
  day: string;
  shots: number;
}

export function Stats() {
  const stats = useStore((s) => s.stats);
  const [weeklyData, setWeeklyData] = useState<WeeklyData[]>([]);
  const [dailyStats, setDailyStats] = useState<DailyStats[]>([]);
  const [loading, setLoading] = useState(false);

  // Fetch extended stats
  useEffect(() => {
    fetchExtendedStats();
  }, []);

  const fetchExtendedStats = async () => {
    setLoading(true);
    try {
      // Try to fetch from API
      const response = await fetch('/api/stats/extended');
      if (response.ok) {
        const data = await response.json();
        if (data.weekly) setWeeklyData(data.weekly);
        if (data.daily) setDailyStats(data.daily);
      }
    } catch {
      // Generate mock data for display
      generateMockData();
    }
    setLoading(false);
  };

  const generateMockData = () => {
    const days = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const weekly = days.map(day => ({
      day,
      shots: Math.floor(Math.random() * 5) + 1,
    }));
    setWeeklyData(weekly);
  };

  const markCleaning = async () => {
    try {
      await fetch('/api/stats/cleaning', { method: 'POST' });
      // Refresh stats
      fetchExtendedStats();
    } catch {
      console.error('Failed to mark cleaning');
    }
  };

  // Calculate averages
  const avgShotsPerDay = stats.totalShots > 0 
    ? (stats.totalShots / Math.max(1, Math.ceil((Date.now() - (stats.firstShotDate || Date.now())) / (1000 * 60 * 60 * 24)))).toFixed(1)
    : '0';

  const maxShots = weeklyData.length > 0 
    ? Math.max(...weeklyData.map(d => d.shots), 1) 
    : 5;

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-coffee-900">Statistics</h1>
          <p className="text-coffee-500 mt-1">Track your brewing journey</p>
        </div>
        <Button variant="secondary" onClick={fetchExtendedStats} disabled={loading}>
          <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
          Refresh
        </Button>
      </div>

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
          label="Since Cleaning"
          value={stats.shotsSinceCleaning}
          warning={stats.shotsSinceCleaning > 100}
          subtext={stats.shotsSinceCleaning > 100 ? 'Time to clean!' : 'Looking good'}
          color={stats.shotsSinceCleaning > 100 ? 'amber' : 'blue'}
        />
        <MetricCard
          icon={<Timer className="w-5 h-5" />}
          label="Avg Shot Time"
          value={stats.avgShotTime ? `${stats.avgShotTime.toFixed(1)}s` : '—'}
          subtext="Last 10 shots"
          color="purple"
        />
      </div>

      {/* Weekly Chart */}
      <Card>
        <CardHeader>
          <CardTitle icon={<BarChart3 className="w-5 h-5" />}>
            This Week
          </CardTitle>
        </CardHeader>

        <div className="h-48">
          {weeklyData.length > 0 ? (
            <div className="flex items-end justify-between h-full gap-2 pt-4">
              {weeklyData.map((data, index) => (
                <div key={data.day} className="flex-1 flex flex-col items-center">
                  <div className="w-full flex flex-col items-center justify-end h-32">
                    <span className="text-xs font-semibold text-coffee-700 mb-1">
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
                  <span className="text-xs text-coffee-500 mt-2 font-medium">{data.day}</span>
                </div>
              ))}
            </div>
          ) : (
            <div className="flex items-center justify-center h-full text-coffee-400">
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
              Brewing Performance
            </CardTitle>
          </CardHeader>

          <div className="space-y-4">
            <StatRow 
              label="Average Yield" 
              value={stats.avgYield ? `${stats.avgYield.toFixed(1)}g` : '—'} 
              icon={<Droplets className="w-4 h-4" />}
            />
            <StatRow 
              label="Average Ratio" 
              value={stats.avgRatio ? `1:${stats.avgRatio.toFixed(1)}` : '—'} 
              icon={<TrendingUp className="w-4 h-4" />}
            />
            <StatRow 
              label="Best Shot Time" 
              value={stats.bestShotTime ? `${stats.bestShotTime}s` : '—'} 
              icon={<Timer className="w-4 h-4" />}
            />
            <StatRow 
              label="Consistency Score" 
              value={stats.consistencyScore ? `${stats.consistencyScore}%` : '—'} 
              icon={<Sparkles className="w-4 h-4" />}
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
              value={formatRuntime(stats.totalRuntime || 0)} 
              icon={<Clock className="w-4 h-4" />}
            />
            <StatRow 
              label="Heating Cycles" 
              value={stats.heatingCycles?.toLocaleString() || '—'} 
              icon={<Flame className="w-4 h-4" />}
            />
            <StatRow 
              label="Energy Used" 
              value={stats.energyUsed ? `${stats.energyUsed.toFixed(1)} kWh` : '—'} 
              icon={<Zap className="w-4 h-4" />}
            />
            <StatRow 
              label="Water Used" 
              value={stats.waterUsed ? `${(stats.waterUsed / 1000).toFixed(1)} L` : '—'} 
              icon={<Droplets className="w-4 h-4" />}
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
          <div className="p-4 bg-cream-100 rounded-xl">
            <div className="text-sm text-coffee-500 mb-1">Last Cleaning</div>
            <div className="font-semibold text-coffee-900">
              {stats.lastCleaning 
                ? new Date(stats.lastCleaning).toLocaleDateString(undefined, { 
                    month: 'short', 
                    day: 'numeric',
                    year: 'numeric'
                  }) 
                : 'Never'}
            </div>
          </div>
          <div className="p-4 bg-cream-100 rounded-xl">
            <div className="text-sm text-coffee-500 mb-1">Shots Since</div>
            <div className={`font-semibold ${stats.shotsSinceCleaning > 100 ? 'text-amber-600' : 'text-coffee-900'}`}>
              {stats.shotsSinceCleaning} shots
            </div>
          </div>
          <div className="p-4 bg-cream-100 rounded-xl">
            <div className="text-sm text-coffee-500 mb-1">Next Clean</div>
            <div className="font-semibold text-coffee-900">
              {stats.shotsSinceCleaning > 100 
                ? 'Now!' 
                : `In ~${Math.max(0, 100 - stats.shotsSinceCleaning)} shots`}
            </div>
          </div>
        </div>

        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4 p-4 bg-cream-50 rounded-xl">
          <div>
            <h4 className="font-semibold text-coffee-800">Completed a cleaning cycle?</h4>
            <p className="text-sm text-coffee-500">
              Mark it to reset the counter and keep track of maintenance.
            </p>
          </div>
          <Button onClick={markCleaning}>
            <Sparkles className="w-4 h-4" />
            Mark as Cleaned
          </Button>
        </div>
      </Card>

      {/* Achievements / Milestones */}
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
  const colorClasses = {
    accent: 'from-accent/10 to-accent/5 text-accent',
    emerald: 'from-emerald-500/10 to-emerald-500/5 text-emerald-600',
    amber: 'from-amber-500/10 to-amber-500/5 text-amber-600',
    blue: 'from-blue-500/10 to-blue-500/5 text-blue-600',
    purple: 'from-purple-500/10 to-purple-500/5 text-purple-600',
  };

  return (
    <Card className={`bg-gradient-to-br ${colorClasses[color].split(' ')[0]} ${colorClasses[color].split(' ')[1]}`}>
      <div className="flex items-start justify-between mb-3">
        <div className={`p-2 rounded-lg bg-white/60 ${colorClasses[color].split(' ')[2]}`}>
          {icon}
        </div>
        {warning && <Badge variant="warning">!</Badge>}
      </div>
      <div className={`text-3xl font-bold ${warning ? 'text-amber-600' : 'text-coffee-900'}`}>
        {value}
      </div>
      <div className="text-sm text-coffee-500 mt-1">{label}</div>
      {subtext && (
        <div className="text-xs text-coffee-400 mt-0.5">{subtext}</div>
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
    <div className="flex items-center justify-between py-2 border-b border-cream-200 last:border-0">
      <div className="flex items-center gap-2 text-coffee-500">
        {icon}
        <span className="text-sm">{label}</span>
      </div>
      <span className="text-sm font-semibold text-coffee-900">{value}</span>
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
        : 'bg-cream-100 opacity-50'
    }`}>
      <div className="text-2xl mb-2">{icon}</div>
      <div className={`text-sm font-medium ${achieved ? 'text-coffee-800' : 'text-coffee-400'}`}>
        {label}
      </div>
      {achieved && (
        <Badge variant="success" className="mt-2">Achieved</Badge>
      )}
    </div>
  );
}

function formatRuntime(seconds: number): string {
  if (seconds < 3600) {
    return `${Math.floor(seconds / 60)}m`;
  } else if (seconds < 86400) {
    return `${Math.floor(seconds / 3600)}h ${Math.floor((seconds % 3600) / 60)}m`;
  } else {
    return `${Math.floor(seconds / 86400)}d ${Math.floor((seconds % 86400) / 3600)}h`;
  }
}

