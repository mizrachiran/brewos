import { useState } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Gauge } from '@/components/Gauge';
import { Badge } from '@/components/Badge';
import { PageHeader } from '@/components/PageHeader';
import { HeatingStrategyModal } from '@/components/HeatingStrategyModal';
import { 
  Flame, 
  Wind, 
  Gauge as GaugeIcon, 
  Zap, 
  Droplets,
  Coffee,
  Clock,
  Scale,
  Power,
} from 'lucide-react';
import { formatUptime, getMachineStateLabel, getMachineStateColor } from '@/lib/utils';

export function Dashboard() {
  const machine = useStore((s) => s.machine);
  const temps = useStore((s) => s.temps);
  const pressure = useStore((s) => s.pressure);
  const power = useStore((s) => s.power);
  const water = useStore((s) => s.water);
  const scale = useStore((s) => s.scale);
  const stats = useStore((s) => s.stats);
  const esp32 = useStore((s) => s.esp32);

  const [showStrategyModal, setShowStrategyModal] = useState(false);

  const setMode = (mode: string, strategy?: number) => {
    if (mode === 'on' && strategy !== undefined) {
      // Send mode and strategy together
      getConnection()?.sendCommand('set_mode', { mode, strategy });
    } else {
      getConnection()?.sendCommand('set_mode', { mode });
    }
  };

  const handleOnClick = () => {
    // If machine is already on, just turn it on without modal
    if (machine.mode === 'on') {
      setMode('on');
    } else {
      // Show strategy selection modal when turning on from standby/eco
      setShowStrategyModal(true);
    }
  };

  const handleStrategySelect = (strategy: number) => {
    setMode('on', strategy);
    setShowStrategyModal(false);
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <PageHeader
        title="Dashboard"
        subtitle="Monitor your machine status"
      />

      {/* Machine Status */}
      <Card>
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
          <div>
            <h2 className="text-2xl font-bold text-theme mb-2">Machine Status</h2>
            <Badge className={getMachineStateColor(machine.state)}>
              {getMachineStateLabel(machine.state)}
            </Badge>
          </div>
          
          <div className="flex gap-2">
            <button
              onClick={() => setMode('standby')}
              className={`
                px-4 py-2 rounded-xl text-sm font-semibold transition-all
                ${machine.mode === 'standby'
                  ? 'nav-active'
                  : 'bg-theme-secondary text-theme-secondary hover:bg-theme-tertiary'
                }
              `}
            >
              <Power className="w-4 h-4 inline mr-1.5" />
              Standby
            </button>
            <button
              onClick={handleOnClick}
              className={`
                px-4 py-2 rounded-xl text-sm font-semibold transition-all
                ${machine.mode === 'on'
                  ? 'nav-active'
                  : 'bg-theme-secondary text-theme-secondary hover:bg-theme-tertiary'
                }
              `}
            >
              On
            </button>
            <button
              onClick={() => setMode('eco')}
              className={`
                px-4 py-2 rounded-xl text-sm font-semibold transition-all
                ${machine.mode === 'eco'
                  ? 'nav-active'
                  : 'bg-theme-secondary text-theme-secondary hover:bg-theme-tertiary'
                }
              `}
            >
              Eco
            </button>
          </div>
        </div>
      </Card>

      {/* Temperature Gauges */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <Gauge
          value={temps.brew.current}
          max={temps.brew.max}
          setpoint={temps.brew.setpoint}
          label="Brew Boiler"
          unit="°C"
          icon={<Flame className="w-5 h-5" />}
          variant="default"
        />
        <Gauge
          value={temps.steam.current}
          max={temps.steam.max}
          setpoint={temps.steam.setpoint}
          label="Steam Boiler"
          unit="°C"
          icon={<Wind className="w-5 h-5" />}
          variant="steam"
        />
      </div>

      {/* Pressure & Power */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* Pressure */}
        <Card>
          <CardHeader>
            <CardTitle icon={<GaugeIcon className="w-5 h-5" />}>Pressure</CardTitle>
          </CardHeader>
          <div className="flex items-baseline gap-2 mb-4">
            <span className="text-5xl font-bold text-accent tabular-nums">
              {pressure.toFixed(1)}
            </span>
            <span className="text-2xl text-theme-muted">bar</span>
          </div>
          <div className="relative h-4 bg-theme-secondary rounded-full overflow-hidden">
            <div
              className="absolute inset-y-0 left-0 rounded-full bg-gradient-to-r from-emerald-400 to-cyan-500 transition-all duration-300"
              style={{ width: `${Math.min(100, (pressure / 15) * 100)}%` }}
            />
            {/* Markers */}
            <div className="absolute inset-0 flex justify-between px-1 text-[8px] text-theme-muted">
              {[0, 5, 10, 15].map((mark) => (
                <span key={mark} className="relative top-5">{mark}</span>
              ))}
            </div>
          </div>
        </Card>

        {/* Power */}
        <Card>
          <CardHeader>
            <CardTitle icon={<Zap className="w-5 h-5" />}>Power</CardTitle>
          </CardHeader>
          <div className="flex items-baseline gap-2 mb-4">
            <span className="text-5xl font-bold text-accent tabular-nums">
              {Math.round(power.current)}
            </span>
            <span className="text-2xl text-theme-muted">W</span>
          </div>
          <div className="grid grid-cols-2 gap-4 text-sm">
            <div>
              <span className="text-theme-muted">Today</span>
              <p className="font-semibold text-theme-secondary">{power.todayKwh.toFixed(1)} kWh</p>
            </div>
            <div>
              <span className="text-theme-muted">Voltage</span>
              <p className="font-semibold text-theme-secondary">{power.voltage} V</p>
            </div>
          </div>
        </Card>
      </div>

      {/* Quick Stats */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <QuickStat
          icon={<Coffee className="w-5 h-5" />}
          label="Shots Today"
          value={stats.shotsToday.toString()}
        />
        <QuickStat
          icon={<Clock className="w-5 h-5" />}
          label="Uptime"
          value={formatUptime(esp32.uptime)}
        />
        <QuickStat
          icon={<Droplets className="w-5 h-5" />}
          label="Water Tank"
          value={water.tankLevel.toUpperCase()}
          status={water.tankLevel === 'ok' ? 'success' : water.tankLevel === 'low' ? 'warning' : 'error'}
        />
        <QuickStat
          icon={<Scale className="w-5 h-5" />}
          label="Scale"
          value={scale.connected ? `${scale.weight.toFixed(1)}g` : 'Not connected'}
          status={scale.connected ? 'success' : undefined}
        />
      </div>

      {/* Heating Strategy Modal */}
      <HeatingStrategyModal
        isOpen={showStrategyModal}
        onClose={() => setShowStrategyModal(false)}
        onSelect={handleStrategySelect}
      />
    </div>
  );
}

interface QuickStatProps {
  icon: React.ReactNode;
  label: string;
  value: string;
  status?: 'success' | 'warning' | 'error';
}

function QuickStat({ icon, label, value, status }: QuickStatProps) {
  const statusColors = {
    success: 'text-emerald-500',
    warning: 'text-amber-500',
    error: 'text-red-500',
  };

  return (
    <Card className="flex flex-col items-center justify-between text-center p-4 h-full">
      <span className="text-accent h-6 flex items-center">{icon}</span>
      <span className={`text-lg font-bold leading-tight min-h-[1.75rem] flex items-center ${status ? statusColors[status] : 'text-theme'}`}>
        {value}
      </span>
      <span className="text-xs text-theme-muted h-4">{label}</span>
    </Card>
  );
}
