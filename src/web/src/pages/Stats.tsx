import { useState, useEffect, useCallback } from "react";
import { useStore } from "@/lib/store";
import type {
  ExtendedStatsResponse,
  PowerSample,
  DailySummary,
  BrewRecord,
  Statistics,
} from "@/lib/types";
import { useCommand } from "@/lib/useCommand";
import { useMobileLandscape } from "@/lib/useMobileLandscape";
import { isDemoMode } from "@/lib/demo-mode";
import { getDemoExtendedStats } from "@/lib/demo-stats";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { PageHeader } from "@/components/PageHeader";
import {
  MetricCard,
  StatRow,
  WeeklyChart,
  MaintenanceCard,
  MilestoneCard,
  PowerChart,
  BrewTrendsChart,
  HourlyDistributionChart,
  EnergyTrendsChart,
  type WeeklyData,
  type HourlyData,
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
  Activity,
  Battery,
  Sun,
  History,
} from "lucide-react";

type TabId = "overview" | "power" | "history" | "maintenance";

export function Stats() {
  const stats = useStore((s) => s.stats);
  const { sendCommand } = useCommand();

  const [activeTab, setActiveTab] = useState<TabId>("overview");
  const [loading, setLoading] = useState(false);
  const [weeklyData, setWeeklyData] = useState<WeeklyData[]>([]);
  const [hourlyData, setHourlyData] = useState<HourlyData[]>([]);
  const [powerHistory, setPowerHistory] = useState<PowerSample[]>([]);
  const [dailyHistory, setDailyHistory] = useState<DailySummary[]>([]);
  const [brewHistory, setBrewHistory] = useState<BrewRecord[]>([]);

  const generateWeeklyEstimate = useCallback(() => {
    const days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
    const avgPerDay =
      stats.weeklyCount > 0 ? Math.round(stats.weeklyCount / 7) : 0;
    const weekly = days.map((day, index) => {
      const today = new Date().getDay();
      const dayIndex = (index + 1) % 7;
      return {
        day,
        shots: dayIndex === today ? stats.shotsToday : avgPerDay,
      };
    });
    setWeeklyData(weekly);
  }, [stats.weeklyCount, stats.shotsToday]);

  const fetchExtendedStats = useCallback(async () => {
    setLoading(true);
    try {
      // In demo mode, use generated demo data
      if (isDemoMode()) {
        const demoData = getDemoExtendedStats();
        setWeeklyData(demoData.weekly);
        setHourlyData(demoData.hourlyDistribution);
        setPowerHistory(demoData.powerHistory);
        setDailyHistory(demoData.dailyHistory);
        setBrewHistory(demoData.brewHistory);
        setLoading(false);
        return;
      }

      const response = await fetch("/api/stats/extended");
      if (response.ok) {
        const data: ExtendedStatsResponse = await response.json();
        if (data.weekly) setWeeklyData(data.weekly);
        if (data.hourlyDistribution) setHourlyData(data.hourlyDistribution);
        if (data.powerHistory) setPowerHistory(data.powerHistory);
        if (data.dailyHistory) setDailyHistory(data.dailyHistory);
        if (data.brewHistory) setBrewHistory(data.brewHistory);
      }
    } catch {
      generateWeeklyEstimate();
    }
    setLoading(false);
  }, [generateWeeklyEstimate]);

  useEffect(() => {
    fetchExtendedStats();
  }, [fetchExtendedStats]);

  const markCleaning = (type: "backflush" | "descale") => {
    const typeLabels = {
      backflush: "Backflush & Group Clean",
      descale: "Descale",
    };
    sendCommand(
      "record_maintenance",
      { type },
      {
        successMessage: `${typeLabels[type]} recorded`,
      }
    );
  };

  // Computed values
  const avgShotsPerDay =
    stats.totalOnTimeMinutes > 0 && stats.totalShots > 0
      ? (
          stats.totalShots /
          Math.max(1, Math.ceil(stats.totalOnTimeMinutes / 1440))
        ).toFixed(1)
      : stats.dailyCount > 0
      ? stats.dailyCount.toString()
      : "0";

  const avgShotTimeSec = formatMsToSeconds(stats.avgBrewTimeMs);

  // Calculate days since first shot
  const daysSinceFirstShot = stats.lifetime?.firstShotTimestamp
    ? Math.floor(
        (Date.now() / 1000 - stats.lifetime.firstShotTimestamp) / 86400
      )
    : 0;

  const tabs: { id: TabId; label: string; icon: React.ReactNode }[] = [
    {
      id: "overview",
      label: "Overview",
      icon: <BarChart3 className="w-4 h-4" />,
    },
    { id: "power", label: "Power", icon: <Zap className="w-4 h-4" /> },
    { id: "history", label: "History", icon: <History className="w-4 h-4" /> },
    {
      id: "maintenance",
      label: "Maintenance",
      icon: <Sparkles className="w-4 h-4" />,
    },
  ];

  const isMobileLandscape = useMobileLandscape();
  const sectionGap = isMobileLandscape ? "space-y-3" : "space-y-6";

  return (
    <div className={sectionGap}>
      <PageHeader title="Statistics" subtitle="Track your brewing journey" />

      {/* Tab Navigation */}
      <div className="flex items-center gap-2 p-1 bg-theme-elevated rounded-lg overflow-x-auto">
        <div className="flex gap-1 flex-1">
          {tabs.map((tab) => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`
              flex items-center gap-2 px-4 py-2 rounded-md text-sm font-medium transition-all
              ${
                activeTab === tab.id
                  ? "bg-accent text-white shadow-md"
                  : "text-theme-secondary hover:text-theme-primary hover:bg-theme-surface"
              }
            `}
            >
              {tab.icon}
              <span className="hidden sm:inline">{tab.label}</span>
            </button>
          ))}
        </div>
        <button
          onClick={fetchExtendedStats}
          disabled={loading}
          className="p-2 rounded-md text-theme-muted hover:text-theme hover:bg-theme-surface transition-colors disabled:opacity-50"
          title="Refresh statistics"
        >
          <RefreshCw className={`w-4 h-4 ${loading ? "animate-spin" : ""}`} />
        </button>
      </div>

      {/* Overview Tab */}
      {activeTab === "overview" && (
        <div className="space-y-6">
          {/* Key Metrics */}
          <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
            <MetricCard
              icon={<Coffee className="w-5 h-5" />}
              label="Total Shots"
              value={stats.totalShots.toLocaleString()}
              subtext={
                daysSinceFirstShot > 0
                  ? `over ${daysSinceFirstShot} days`
                  : undefined
              }
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
              icon={<Timer className="w-5 h-5" />}
              label="Avg Shot Time"
              value={avgShotTimeSec ? `${avgShotTimeSec}s` : "—"}
              subtext={
                stats.minBrewTimeMs > 0
                  ? `${formatMsToSeconds(
                      stats.minBrewTimeMs
                    )}-${formatMsToSeconds(stats.maxBrewTimeMs)}s`
                  : undefined
              }
              color="purple"
            />
            <MetricCard
              icon={<Zap className="w-5 h-5" />}
              label="Energy Used"
              value={stats.totalKwh > 0 ? `${stats.totalKwh.toFixed(1)}` : "—"}
              subtext="kWh total"
              color="amber"
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
              <WeeklyChart
                data={weeklyData}
                emptyMessage="No data available yet. Start brewing!"
              />
            </div>
          </Card>

          {/* Quick Stats Grid */}
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            {/* Brewing Stats */}
            <Card>
              <CardHeader>
                <CardTitle icon={<Target className="w-5 h-5" />}>
                  Brewing
                </CardTitle>
              </CardHeader>
              <div className="space-y-4">
                <StatRow
                  label="This Month"
                  value={`${stats.monthlyCount} shots`}
                  icon={<Calendar className="w-4 h-4" />}
                />
                <StatRow
                  label="Steam Cycles"
                  value={stats.totalSteamCycles.toLocaleString()}
                  icon={<Droplets className="w-4 h-4" />}
                />
                <StatRow
                  label="Session Shots"
                  value={stats.sessionShots.toString()}
                  icon={<Coffee className="w-4 h-4" />}
                />
                <StatRow
                  label="Total Runtime"
                  value={formatRuntime(stats.totalOnTimeMinutes * 60)}
                  icon={<Clock className="w-4 h-4" />}
                />
              </div>
            </Card>

            {/* When You Brew */}
            <Card>
              <CardHeader>
                <CardTitle icon={<Sun className="w-5 h-5" />}>
                  When You Brew
                </CardTitle>
              </CardHeader>
              <HourlyDistributionChart
                data={hourlyData}
                height={160}
                emptyMessage="Brew some shots to see your schedule!"
              />
            </Card>
          </div>

          {/* Milestones */}
          {stats.totalShots > 0 && (
            <MilestonesCard totalShots={stats.totalShots} />
          )}
        </div>
      )}

      {/* Power Tab */}
      {activeTab === "power" && (
        <PowerTab
          stats={stats}
          powerHistory={powerHistory}
          dailyHistory={dailyHistory}
        />
      )}

      {/* History Tab */}
      {activeTab === "history" && (
        <div className="space-y-6">
          {/* Trends Chart */}
          <Card>
            <CardHeader>
              <CardTitle icon={<TrendingUp className="w-5 h-5" />}>
                30-Day Trends
              </CardTitle>
            </CardHeader>
            <div className="px-4 pb-4">
              <BrewTrendsChart
                data={dailyHistory}
                height={200}
                emptyMessage="Trend data will appear as you use your machine"
              />
            </div>
          </Card>

          {/* Recent Brews */}
          <Card>
            <CardHeader>
              <CardTitle icon={<History className="w-5 h-5" />}>
                Recent Brews
              </CardTitle>
            </CardHeader>
            {brewHistory.length > 0 ? (
              <div className="divide-y divide-theme-border">
                {brewHistory.slice(0, 20).map((brew, i) => (
                  <BrewHistoryRow key={i} brew={brew} />
                ))}
              </div>
            ) : (
              <div className="py-12 text-center text-theme-muted">
                <Coffee className="w-12 h-12 mx-auto mb-3 opacity-40" />
                <p>No brew history yet</p>
                <p className="text-sm mt-1">Your shots will appear here</p>
              </div>
            )}
          </Card>
        </div>
      )}

      {/* Maintenance Tab */}
      {activeTab === "maintenance" && (
        <div className="space-y-6">
          {/* Maintenance Cards */}
          <Card>
            <CardHeader>
              <CardTitle icon={<Sparkles className="w-5 h-5" />}>
                Maintenance Schedule
              </CardTitle>
            </CardHeader>
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
              <MaintenanceCard
                label="Backflush & Group Clean"
                description="Chemical clean with group brush"
                shotsSince={stats.shotsSinceBackflush}
                lastTimestamp={stats.lastBackflushTimestamp}
                threshold={100}
                warningThreshold={80}
                icon={<Sparkles className="w-5 h-5" />}
                onMark={() => markCleaning("backflush")}
              />
              <MaintenanceCard
                label="Descale"
                description="Remove mineral buildup from boiler"
                shotsSince={stats.shotsSinceDescale}
                lastTimestamp={stats.lastDescaleTimestamp}
                threshold={500}
                warningThreshold={400}
                icon={<Droplets className="w-5 h-5" />}
                onMark={() => markCleaning("descale")}
              />
            </div>
          </Card>

          {/* Maintenance Tips */}
          <Card>
            <CardHeader>
              <CardTitle icon={<Target className="w-5 h-5" />}>
                Maintenance Tips
              </CardTitle>
            </CardHeader>
            <div className="space-y-4 text-sm">
              <TipRow
                title="Water Backflush"
                description="Run a quick backflush with plain water (blank basket) after your last shot of the day."
                frequency="Daily"
              />
              <TipRow
                title="Chemical Backflush"
                description="Use a cleaning tablet/powder with your blind basket to remove coffee oils from the group."
                frequency="Every 50-100 shots"
              />
              <TipRow
                title="Descaling"
                description="Remove mineral buildup from your boilers with a descaling solution."
                frequency="Every 2-3 months"
              />
            </div>
          </Card>
        </div>
      )}
    </div>
  );
}

// Sub-components

interface BrewHistoryRowProps {
  brew: BrewRecord;
}

function BrewHistoryRow({ brew }: BrewHistoryRowProps) {
  const date = new Date(brew.timestamp * 1000);
  const timeStr = date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
  });
  const dateStr = date.toLocaleDateString([], {
    month: "short",
    day: "numeric",
  });

  return (
    <div className="flex items-center justify-between py-3 px-4 hover:bg-theme-surface/50 transition-colors">
      <div className="flex items-center gap-3">
        <div className="w-10 h-10 rounded-full bg-accent/10 flex items-center justify-center">
          <Coffee className="w-5 h-5 text-accent" />
        </div>
        <div>
          <div className="font-medium">
            {(brew.durationMs / 1000).toFixed(1)}s shot
          </div>
          <div className="text-xs text-theme-muted">
            {dateStr} at {timeStr}
          </div>
        </div>
      </div>
      <div className="text-right">
        {brew.yieldWeight > 0 && (
          <div className="font-medium">{brew.yieldWeight.toFixed(1)}g</div>
        )}
        {brew.ratio && brew.ratio > 0 && (
          <div className="text-xs text-theme-muted">
            1:{brew.ratio.toFixed(1)}
          </div>
        )}
      </div>
    </div>
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
    { label: "500 Shots", threshold: 500, icon: "⭐" },
    { label: "1000 Shots", threshold: 1000, icon: "👑" },
    { label: "5000 Shots", threshold: 5000, icon: "🏅" },
  ];

  // Show achieved + next 2 unachieved
  const achievedMilestones = milestones.filter(
    (m) => totalShots >= m.threshold
  );
  const upcomingMilestones = milestones
    .filter((m) => totalShots < m.threshold)
    .slice(0, 2);
  const displayMilestones = [
    ...achievedMilestones.slice(-4),
    ...upcomingMilestones,
  ];

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<TrendingUp className="w-5 h-5" />}>
          Milestones
        </CardTitle>
      </CardHeader>
      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-4">
        {displayMilestones.map((m) => (
          <MilestoneCard
            key={m.threshold}
            label={m.label}
            achieved={totalShots >= m.threshold}
            icon={m.icon}
            progress={
              totalShots < m.threshold
                ? (totalShots / m.threshold) * 100
                : undefined
            }
          />
        ))}
      </div>
    </Card>
  );
}

interface TipRowProps {
  title: string;
  description: string;
  frequency: string;
}

function TipRow({ title, description, frequency }: TipRowProps) {
  return (
    <div className="p-4 rounded-lg bg-theme-surface">
      <div className="flex items-center justify-between mb-1">
        <span className="font-medium">{title}</span>
        <span className="text-xs text-accent">{frequency}</span>
      </div>
      <p className="text-theme-muted text-sm">{description}</p>
    </div>
  );
}

import { getCurrencySymbol } from "@/lib/currency";

interface PowerTabProps {
  stats: Statistics;
  powerHistory: PowerSample[];
  dailyHistory: DailySummary[];
}

function PowerTab({ stats, powerHistory, dailyHistory }: PowerTabProps) {
  const { electricityPrice, currency } = useStore((s) => s.preferences);
  const currencySymbol = getCurrencySymbol(currency);

  // Calculate costs
  const todayCost = (stats.daily?.totalKwh ?? 0) * electricityPrice;
  const weekCost = (stats.weekly?.totalKwh ?? 0) * electricityPrice;
  const monthCost = (stats.monthly?.totalKwh ?? 0) * electricityPrice;
  const lifetimeCost = stats.totalKwh * electricityPrice;

  // Calculate average cost per shot
  const avgCostPerShot =
    stats.totalShots > 0 ? lifetimeCost / stats.totalShots : 0;

  return (
    <div className="space-y-6">
      {/* Energy Metrics */}
      <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
        <MetricCard
          icon={<Zap className="w-5 h-5" />}
          label="Lifetime Energy"
          value={`${stats.totalKwh.toFixed(2)}`}
          subtext="kWh total"
          color="amber"
        />
        <MetricCard
          icon={<Activity className="w-5 h-5" />}
          label="Today"
          value={`${stats.daily?.totalKwh?.toFixed(2) ?? "0.00"}`}
          subtext="kWh"
          color="emerald"
        />
        <MetricCard
          icon={<Battery className="w-5 h-5" />}
          label="This Week"
          value={`${stats.weekly?.totalKwh?.toFixed(2) ?? "0.00"}`}
          subtext="kWh"
          color="blue"
        />
        <MetricCard
          icon={<Flame className="w-5 h-5" />}
          label="This Month"
          value={`${stats.monthly?.totalKwh?.toFixed(2) ?? "0.00"}`}
          subtext="kWh"
          color="purple"
        />
      </div>

      {/* Cost Metrics */}
      <Card>
        <CardHeader>
          <CardTitle
            icon={<span className="text-lg font-bold">{currencySymbol}</span>}
          >
            Energy Costs
          </CardTitle>
        </CardHeader>
        <p className="text-sm text-theme-muted mb-4">
          Based on your configured rate of {currencySymbol}
          {electricityPrice.toFixed(2)}/kWh
        </p>
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
          <CostCard
            label="Today"
            value={todayCost}
            symbol={currencySymbol}
            color="emerald"
          />
          <CostCard
            label="This Week"
            value={weekCost}
            symbol={currencySymbol}
            color="blue"
          />
          <CostCard
            label="This Month"
            value={monthCost}
            symbol={currencySymbol}
            color="purple"
          />
          <CostCard
            label="Lifetime"
            value={lifetimeCost}
            symbol={currencySymbol}
            color="amber"
          />
        </div>

        {/* Cost per shot insight */}
        {stats.totalShots > 0 && (
          <div className="mt-4 p-4 bg-theme-surface rounded-xl">
            <div className="flex items-center justify-between">
              <div>
                <div className="text-sm text-theme-muted">
                  Average Cost per Shot
                </div>
                <div className="text-2xl font-bold text-theme">
                  {currencySymbol}
                  {avgCostPerShot.toFixed(3)}
                </div>
              </div>
              <div className="text-right text-sm text-theme-muted">
                <div>Based on {stats.totalShots.toLocaleString()} shots</div>
                <div>and {stats.totalKwh.toFixed(2)} kWh used</div>
              </div>
            </div>
          </div>
        )}
      </Card>

      {/* Power Consumption Chart */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Activity className="w-5 h-5" />}>
            Power Consumption (24h)
          </CardTitle>
        </CardHeader>
        <div className="px-4 pb-4">
          <PowerChart
            data={powerHistory}
            height={240}
            emptyMessage="Power data will appear as you use your machine"
          />
        </div>
      </Card>

      {/* Energy Trends */}
      <Card>
        <CardHeader>
          <CardTitle icon={<TrendingUp className="w-5 h-5" />}>
            Energy Trends (30 days)
          </CardTitle>
        </CardHeader>
        <div className="px-4 pb-4">
          <EnergyTrendsChart
            data={dailyHistory}
            height={220}
            emptyMessage="Energy trends will appear over time"
          />
        </div>
      </Card>
    </div>
  );
}

interface CostCardProps {
  label: string;
  value: number;
  symbol: string;
  color: "emerald" | "blue" | "purple" | "amber";
}

function CostCard({ label, value, symbol, color }: CostCardProps) {
  const colorClasses = {
    emerald: "bg-emerald-500/10 text-emerald-600 dark:text-emerald-400",
    blue: "bg-blue-500/10 text-blue-600 dark:text-blue-400",
    purple: "bg-purple-500/10 text-purple-600 dark:text-purple-400",
    amber: "bg-amber-500/10 text-amber-600 dark:text-amber-400",
  };

  return (
    <div className={`p-4 rounded-xl ${colorClasses[color]}`}>
      <div className="text-xs font-medium opacity-80 mb-1">{label}</div>
      <div className="text-2xl font-bold tabular-nums">
        {symbol}
        {value.toFixed(2)}
      </div>
    </div>
  );
}
