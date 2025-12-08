import { useState, useCallback, useMemo, useEffect } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { PageHeader } from "@/components/PageHeader";
import { StatusBar } from "@/components/StatusBar";
import { HeatingStrategyModal } from "@/components/HeatingStrategyModal";
import {
  MachineStatusCard,
  TemperatureGauges,
  PressureCard,
  PowerCard,
  QuickStat,
  LastShotCard,
} from "@/components/dashboard";
import { Sparkles, Clock, Droplets, Scale } from "lucide-react";
import { formatUptime } from "@/lib/utils";

// Time to show pressure card after brewing ends (ms)
const POST_BREW_PRESSURE_DISPLAY_MS = 30000; // 30 seconds

export function Dashboard() {
  // Use specific selectors to prevent unnecessary re-renders
  const machineMode = useStore((s) => s.machine.mode);
  const machineState = useStore((s) => s.machine.state);
  const machineType = useStore((s) => s.device.machineType);
  const heatingStrategy = useStore((s) => s.machine.heatingStrategy);
  const isBrewing = useStore((s) => s.machine.isBrewing);
  const shotActive = useStore((s) => s.shot.active);
  const lastShotTimestamp = useStore((s) => s.machine.lastShotTimestamp);

  // Temperature data - individual primitive selectors to prevent unnecessary re-renders
  const brewTempCurrent = useStore(
    (s) => Math.round(s.temps.brew.current * 10) / 10
  );
  const brewTempSetpoint = useStore((s) => s.temps.brew.setpoint);
  const brewTempMax = useStore((s) => s.temps.brew.max);
  const steamTempCurrent = useStore(
    (s) => Math.round(s.temps.steam.current * 10) / 10
  );
  const steamTempSetpoint = useStore((s) => s.temps.steam.setpoint);
  const steamTempMax = useStore((s) => s.temps.steam.max);
  const groupTemp = useStore((s) => Math.round(s.temps.group * 10) / 10);

  // Memoize temperature objects to prevent child re-renders
  const brewTemp = useMemo(
    () => ({
      current: brewTempCurrent,
      setpoint: brewTempSetpoint,
      max: brewTempMax,
    }),
    [brewTempCurrent, brewTempSetpoint, brewTempMax]
  );

  const steamTemp = useMemo(
    () => ({
      current: steamTempCurrent,
      setpoint: steamTempSetpoint,
      max: steamTempMax,
    }),
    [steamTempCurrent, steamTempSetpoint, steamTempMax]
  );

  // Other values - use specific selectors
  const pressure = useStore((s) => Math.round(s.pressure * 10) / 10);
  const powerCurrent = useStore((s) => Math.round(s.power.current));
  const powerTodayKwh = useStore((s) => Math.round(s.power.todayKwh * 10) / 10);
  const powerVoltage = useStore((s) => s.power.voltage);
  const waterTankLevel = useStore((s) => s.water.tankLevel);
  const scaleConnected = useStore((s) => s.scale.connected);
  const scaleWeight = useStore((s) => Math.round(s.scale.weight * 10) / 10);
  const scaleBattery = useStore((s) => s.scale.battery);
  const machineOnTimestamp = useStore((s) => s.machine.machineOnTimestamp);
  const shotsSinceBackflush = useStore(
    (s) => s.stats.maintenance.shotsSinceBackflush
  );
  const cleaningReminderDue = useStore((s) => s.cleaning.reminderDue);
  const hasPressureSensor = useStore((s) => s.device.hasPressureSensor);

  // Track whether to show pressure card (during and shortly after brewing, and only if sensor exists)
  const [showPressureCard, setShowPressureCard] = useState(false);

  useEffect(() => {
    // No pressure sensor installed - never show pressure card
    if (!hasPressureSensor) {
      setShowPressureCard(false);
      return;
    }

    // Show pressure card while brewing
    if (isBrewing || shotActive) {
      setShowPressureCard(true);
      return;
    }

    // After brewing ends, show pressure for 30 seconds then hide
    if (lastShotTimestamp) {
      const timeSinceLastShot = Date.now() - lastShotTimestamp;
      if (timeSinceLastShot < POST_BREW_PRESSURE_DISPLAY_MS) {
        setShowPressureCard(true);
        const timeout = setTimeout(() => {
          setShowPressureCard(false);
        }, POST_BREW_PRESSURE_DISPLAY_MS - timeSinceLastShot);
        return () => clearTimeout(timeout);
      }
    }

    // Not brewing and no recent shot - hide pressure
    setShowPressureCard(false);
  }, [isBrewing, shotActive, lastShotTimestamp, hasPressureSensor]);

  // Calculate uptime locally for smooth updates
  const [uptime, setUptime] = useState(0);

  useEffect(() => {
    // Update uptime every second when machine is on
    const updateUptime = () => {
      if (machineOnTimestamp) {
        setUptime(Date.now() - machineOnTimestamp);
      } else {
        setUptime(0);
      }
    };

    // Initial update
    updateUptime();

    // Update every second
    const interval = setInterval(updateUptime, 1000);
    return () => clearInterval(interval);
  }, [machineOnTimestamp]);

  const { sendCommand } = useCommand();
  const [showStrategyModal, setShowStrategyModal] = useState(false);

  const isDualBoiler = machineType === "dual_boiler";

  const setMode = useCallback(
    (mode: string, strategy?: number) => {
      const payload =
        mode === "on" && strategy !== undefined ? { mode, strategy } : { mode };
      sendCommand("set_mode", payload);
    },
    [sendCommand]
  );

  // Quick on: use last strategy from localStorage
  const handleQuickOn = useCallback(() => {
    if (isDualBoiler) {
      const stored = localStorage.getItem("brewos-last-heating-strategy");
      const parsed = stored !== null ? parseInt(stored, 10) : NaN;
      const strategy = !isNaN(parsed) ? parsed : 1; // Default to Sequential
      setMode("on", strategy);
    } else {
      setMode("on");
    }
  }, [isDualBoiler, setMode]);

  // Open strategy selector dialog
  const handleOpenStrategyModal = useCallback(() => {
    setShowStrategyModal(true);
  }, []);

  const handleStrategySelect = useCallback(
    (strategy: number) => {
      setMode("on", strategy);
      setShowStrategyModal(false);
    },
    [setMode]
  );

  // Memoize formatted values
  const formattedUptime = useMemo(() => formatUptime(uptime), [uptime]);
  const scaleDisplayValue = useMemo(
    () => (scaleConnected ? `${scaleWeight.toFixed(1)}g` : "Not connected"),
    [scaleConnected, scaleWeight]
  );

  // Determine scale subtext
  const scaleSubtext = useMemo(() => {
    if (!scaleConnected) return undefined;
    if (scaleBattery > 0) return `${scaleBattery}% battery`;
    return undefined;
  }, [scaleConnected, scaleBattery]);

  // Water tank status helpers
  const waterStatus = useMemo(() => {
    if (waterTankLevel === "ok") return "success" as const;
    if (waterTankLevel === "low") return "warning" as const;
    return "error" as const;
  }, [waterTankLevel]);

  const waterDisplayValue = useMemo(() => {
    if (waterTankLevel === "ok") return "OK";
    if (waterTankLevel === "low") return "Low";
    return "Empty";
  }, [waterTankLevel]);

  // Cleaning status helpers
  const cleaningStatus = useMemo(() => {
    if (cleaningReminderDue) return "warning" as const;
    if (shotsSinceBackflush > 150) return "warning" as const;
    return "success" as const;
  }, [cleaningReminderDue, shotsSinceBackflush]);

  const cleaningDisplayValue = useMemo(() => {
    if (cleaningReminderDue) return "Due";
    return `${shotsSinceBackflush} shots`;
  }, [cleaningReminderDue, shotsSinceBackflush]);

  const cleaningSubtext = useMemo(() => {
    if (cleaningReminderDue) return "Backflush needed";
    if (shotsSinceBackflush > 150) return "Consider backflush";
    return "Since backflush";
  }, [cleaningReminderDue, shotsSinceBackflush]);

  return (
    <>
      <div className="space-y-6">
        <PageHeader
          title="Dashboard"
          subtitle="Monitor your machine status"
          action={<StatusBar />}
        />

        <MachineStatusCard
          mode={machineMode}
          state={machineState}
          isDualBoiler={isDualBoiler}
          heatingStrategy={heatingStrategy}
          onSetMode={setMode}
          onQuickOn={handleQuickOn}
          onOpenStrategyModal={handleOpenStrategyModal}
        />

        <TemperatureGauges
          machineType={machineType}
          brewTemp={brewTemp}
          steamTemp={steamTemp}
          groupTemp={groupTemp}
        />

        {/* Contextual Cards: Pressure during brew, Last Shot when idle */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
          {showPressureCard ? (
            <PressureCard pressure={pressure} />
          ) : (
            <LastShotCard />
          )}
          <PowerCard
            current={powerCurrent}
            todayKwh={powerTodayKwh}
            voltage={powerVoltage}
          />
        </div>

        {/* Quick Stats - Machine health at a glance */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          {/* Session uptime */}
          <QuickStat
            icon={<Clock className="w-5 h-5" />}
            label="Session"
            value={machineMode !== "standby" ? formattedUptime : "Off"}
          />

          {/* Maintenance status */}
          <QuickStat
            icon={<Sparkles className="w-5 h-5" />}
            label="Cleaning"
            value={cleaningDisplayValue}
            status={cleaningStatus}
            showPulse={cleaningReminderDue}
            subtext={cleaningSubtext}
          />

          {/* Machine status */}
          <QuickStat
            icon={<Droplets className="w-5 h-5" />}
            label="Water Tank"
            value={waterDisplayValue}
            status={waterStatus}
            showPulse={waterTankLevel !== "ok"}
            subtext={
              waterTankLevel === "low"
                ? "Refill soon"
                : waterTankLevel === "empty"
                ? "Refill now!"
                : undefined
            }
          />
          <QuickStat
            icon={<Scale className="w-5 h-5" />}
            label="Scale"
            value={scaleDisplayValue}
            status={scaleConnected ? "success" : undefined}
            subtext={scaleSubtext}
          />
        </div>
      </div>

      <HeatingStrategyModal
        isOpen={showStrategyModal}
        onClose={() => setShowStrategyModal(false)}
        onSelect={handleStrategySelect}
      />
    </>
  );
}
