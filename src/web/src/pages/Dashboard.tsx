import { useState } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { PageHeader } from "@/components/PageHeader";
import { HeatingStrategyModal } from "@/components/HeatingStrategyModal";
import {
  MachineStatusCard,
  TemperatureGauges,
  PressureCard,
  PowerCard,
  QuickStat,
} from "@/components/dashboard";
import { Coffee, Clock, Droplets, Scale } from "lucide-react";
import { formatUptime } from "@/lib/utils";

export function Dashboard() {
  const machine = useStore((s) => s.machine);
  const temps = useStore((s) => s.temps);
  const pressure = useStore((s) => s.pressure);
  const power = useStore((s) => s.power);
  const water = useStore((s) => s.water);
  const scale = useStore((s) => s.scale);
  const stats = useStore((s) => s.stats);
  const esp32 = useStore((s) => s.esp32);
  const device = useStore((s) => s.device);
  const { sendCommand } = useCommand();

  const [showStrategyModal, setShowStrategyModal] = useState(false);

  const isDualBoiler = device.machineType === "dual_boiler";

  const setMode = (mode: string, strategy?: number) => {
    const payload =
      mode === "on" && strategy !== undefined ? { mode, strategy } : { mode };
    sendCommand("set_mode", payload);
  };

  const handleOnClick = () => {
    if (machine.mode === "on") {
      setMode("on");
    } else if (isDualBoiler) {
      setShowStrategyModal(true);
    } else {
      setMode("on");
    }
  };

  const handleStrategySelect = (strategy: number) => {
    setMode("on", strategy);
    setShowStrategyModal(false);
  };

  return (
    <div className="space-y-6">
      <PageHeader title="Dashboard" subtitle="Monitor your machine status" />

      <MachineStatusCard
        mode={machine.mode}
        state={machine.state}
        isDualBoiler={isDualBoiler}
        onSetMode={setMode}
        onPowerOn={handleOnClick}
      />

      <TemperatureGauges
        machineType={device.machineType}
        brewTemp={temps.brew}
        steamTemp={temps.steam}
        groupTemp={temps.group}
      />

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <PressureCard pressure={pressure} />
        <PowerCard
          current={power.current}
          todayKwh={power.todayKwh}
          voltage={power.voltage}
        />
      </div>

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
          status={
            water.tankLevel === "ok"
              ? "success"
              : water.tankLevel === "low"
              ? "warning"
              : "error"
          }
        />
        <QuickStat
          icon={<Scale className="w-5 h-5" />}
          label="Scale"
          value={scale.connected ? `${scale.weight.toFixed(1)}g` : "Not connected"}
          status={scale.connected ? "success" : undefined}
        />
      </div>

      <HeatingStrategyModal
        isOpen={showStrategyModal}
        onClose={() => setShowStrategyModal(false)}
        onSelect={handleStrategySelect}
      />
    </div>
  );
}
