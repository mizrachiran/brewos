import { useState, useEffect } from "react";
import { useStore } from "@/lib/store";
import type { Statistics } from "@/lib/types";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Button } from "@/components/Button";
import { PageHeader } from "@/components/PageHeader";
import {
  MetricCard,
  StatRow,
  WeeklyChart,
  MaintenanceCard,
  MilestoneCard,
  type WeeklyData,
} from "@/components/stats";
import { formatRuntime, formatMsToSeconds } from "@/lib/format";
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
} from "lucide-react";

export function Stats() {
  const stats = useStore((s) => s.stats);
  const { sendCommand } = useCommand();
  const [weeklyData, setWeeklyData] = useState<WeeklyData[]>([]);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    fetchExtendedStats();
  }, []);

  useEffect(() => {
    if (weeklyData.length === 0 || stats.weeklyCount > 0) {
      generateWeeklyEstimate();
    }
  }, [stats.weeklyCount, stats.shotsToday]);

  const fetchExtendedStats = async () => {
    setLoading(true);
    try {
      const response = await fetch("/api/stats");
      if (response.ok) {
        const data = await response.json();
        if (data.weekly) {
          setWeeklyData(data.weekly);
        }
      }
    } catch {
      generateWeeklyEstimate();
    }
    setLoading(false);
  };

  const generateWeeklyEstimate = () => {
    const days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
    const avgPerDay = stats.weeklyCount > 0 ? Math.round(stats.weeklyCount / 7) : 0;
    const weekly = days.map((day, index) => {
      const today = new Date().getDay();
      const dayIndex = (index + 1) % 7;
      return {
        day,
        shots: dayIndex === today ? stats.shotsToday : avgPerDay,
      };
    });
    setWeeklyData(weekly);
  };

  const markCleaning = (type: "backflush" | "groupClean" | "descale") => {
    const typeLabels = {
      backflush: "Backflush",
      groupClean: "Group clean",
      descale: "Descale",
    };
    sendCommand("record_maintenance", { type }, {
      successMessage: `${typeLabels[type]} recorded`,
    });
  };

  // Computed values
  const avgShotsPerDay =
    stats.totalOnTimeMinutes > 0 && stats.totalShots > 0
      ? (stats.totalShots / Math.max(1, Math.ceil(stats.totalOnTimeMinutes / 1440))).toFixed(1)
      : stats.dailyCount > 0
      ? stats.dailyCount.toString()
      : "0";

  const avgShotTimeSec = formatMsToSeconds(stats.avgBrewTimeMs);

  return (
    <div className="space-y-6">
      <PageHeader
        title="Statistics"
        subtitle="Track your brewing journey"
        action={
          <Button variant="secondary" onClick={fetchExtendedStats} disabled={loading}>
            <RefreshCw className={`w-4 h-4 ${loading ? "animate-spin" : ""}`} />
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
          subtext={stats.shotsSinceBackflush > 100 ? "Time to clean!" : "Looking good"}
          color={stats.shotsSinceBackflush > 100 ? "amber" : "blue"}
        />
        <MetricCard
          icon={<Timer className="w-5 h-5" />}
          label="Avg Shot Time"
          value={avgShotTimeSec ? `${avgShotTimeSec}s` : "—"}
          subtext={
            stats.minBrewTimeMs > 0
              ? `${formatMsToSeconds(stats.minBrewTimeMs)}-${formatMsToSeconds(stats.maxBrewTimeMs)}s range`
              : undefined
          }
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
          <WeeklyChart data={weeklyData} emptyMessage="No data available yet. Start brewing!" />
        </div>
      </Card>

      {/* Detailed Stats Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <BrewingStatsCard stats={stats} avgShotTimeSec={avgShotTimeSec} />
        <MachineUsageCard stats={stats} />
      </div>

      {/* Maintenance */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Sparkles className="w-5 h-5" />}>Maintenance</CardTitle>
        </CardHeader>
        <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 mb-6">
          <MaintenanceCard
            label="Backflush"
            shotsSince={stats.shotsSinceBackflush}
            lastTimestamp={stats.lastBackflushTimestamp}
            threshold={100}
            onMark={() => markCleaning("backflush")}
          />
          <MaintenanceCard
            label="Group Clean"
            shotsSince={stats.shotsSinceGroupClean}
            lastTimestamp={stats.lastGroupCleanTimestamp}
            threshold={50}
            onMark={() => markCleaning("groupClean")}
          />
          <MaintenanceCard
            label="Descale"
            shotsSince={stats.shotsSinceDescale}
            lastTimestamp={stats.lastDescaleTimestamp}
            threshold={500}
            onMark={() => markCleaning("descale")}
          />
        </div>
      </Card>

      {/* Milestones */}
      {stats.totalShots > 0 && <MilestonesCard totalShots={stats.totalShots} />}
    </div>
  );
}

// Sub-components for better organization

interface BrewingStatsCardProps {
  stats: Statistics;
  avgShotTimeSec: string | null;
}

function BrewingStatsCard({ stats, avgShotTimeSec }: BrewingStatsCardProps) {
  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Target className="w-5 h-5" />}>Brewing Stats</CardTitle>
      </CardHeader>
      <div className="space-y-4">
        <StatRow
          label="Average Shot Time"
          value={avgShotTimeSec ? `${avgShotTimeSec}s` : "—"}
          icon={<Timer className="w-4 h-4" />}
        />
        <StatRow
          label="Fastest Shot"
          value={stats.minBrewTimeMs > 0 ? `${formatMsToSeconds(stats.minBrewTimeMs)}s` : "—"}
          icon={<TrendingUp className="w-4 h-4" />}
        />
        <StatRow
          label="Longest Shot"
          value={stats.maxBrewTimeMs > 0 ? `${formatMsToSeconds(stats.maxBrewTimeMs)}s` : "—"}
          icon={<Clock className="w-4 h-4" />}
        />
        <StatRow
          label="This Month"
          value={`${stats.monthlyCount} shots`}
          icon={<Calendar className="w-4 h-4" />}
        />
      </div>
    </Card>
  );
}

interface MachineUsageCardProps {
  stats: Statistics;
}

function MachineUsageCard({ stats }: MachineUsageCardProps) {
  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Flame className="w-5 h-5" />}>Machine Usage</CardTitle>
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
          value={stats.totalKwh > 0 ? `${stats.totalKwh.toFixed(1)} kWh` : "—"}
          icon={<Zap className="w-4 h-4" />}
        />
      </div>
    </Card>
  );
}

interface MilestonesCardProps {
  totalShots: number;
}

function MilestonesCard({ totalShots }: MilestonesCardProps) {
  const milestones = [
    { label: "First Shot", threshold: 1, icon: "☕" },
    { label: "10 Shots", threshold: 10, icon: "🎯" },
    { label: "100 Shots", threshold: 100, icon: "🏆" },
    { label: "1000 Shots", threshold: 1000, icon: "👑" },
  ];

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<TrendingUp className="w-5 h-5" />}>Milestones</CardTitle>
      </CardHeader>
      <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
        {milestones.map((m) => (
          <MilestoneCard
            key={m.threshold}
            label={m.label}
            achieved={totalShots >= m.threshold}
            icon={m.icon}
          />
        ))}
      </div>
    </Card>
  );
}
