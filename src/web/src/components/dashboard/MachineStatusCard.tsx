import { useState, useEffect, useRef, useMemo, memo } from "react";
import { Card } from "@/components/Card";
import { Badge } from "@/components/Badge";
import {
  Power,
  Zap,
  Leaf,
  Coffee,
  Thermometer,
  Flame,
  Wind,
  Brain,
} from "lucide-react";
import { cn, getMachineStateLabel, getMachineStateColor } from "@/lib/utils";
import { useStore } from "@/lib/store";
import { convertFromCelsius, getUnitSymbol } from "@/lib/temperature";
import type { HeatingStrategy } from "@/lib/types";

type MachineMode = "standby" | "on" | "eco";

const STRATEGY_LABELS: Record<number, { label: string; icon: typeof Flame }> = {
  0: { label: "Brew Only", icon: Flame },
  1: { label: "Sequential", icon: Wind },
  2: { label: "Parallel", icon: Zap },
  3: { label: "Smart", icon: Brain },
};

interface MachineStatusCardProps {
  mode: string;
  state: string;
  isDualBoiler: boolean;
  heatingStrategy?: HeatingStrategy | null;
  onSetMode: (mode: string) => void;
  onPowerOn: () => void;
}

export const MachineStatusCard = memo(function MachineStatusCard({
  mode,
  state,
  isDualBoiler,
  heatingStrategy,
  onSetMode,
  onPowerOn,
}: MachineStatusCardProps) {
  // Use specific selectors to avoid re-renders from unrelated store changes
  // Round temperature to 1 decimal to prevent flicker from tiny fluctuations
  const brewCurrent = useStore(
    (s) => Math.round(s.temps.brew.current * 10) / 10
  );
  const brewSetpoint = useStore((s) => s.temps.brew.setpoint);
  const steamCurrent = useStore(
    (s) => Math.round(s.temps.steam.current * 10) / 10
  );
  const steamSetpoint = useStore((s) => s.temps.steam.setpoint);
  const temperatureUnit = useStore((s) => s.preferences.temperatureUnit);

  // Memoize calculated values including combined heating progress for dual boilers
  const { heatingProgress, displayTemp, displaySetpoint, unitSymbol } =
    useMemo(() => {
      // Calculate individual boiler progress
      const brewProgress = Math.min(
        100,
        Math.max(0, (brewCurrent / brewSetpoint) * 100)
      );
      const steamProgress = Math.min(
        100,
        Math.max(0, (steamCurrent / steamSetpoint) * 100)
      );

      // Detect if steam was actually heated (above 50°C means it was heated at some point)
      const steamWasHeated = steamCurrent > 50;

      // Calculate combined progress based on machine type and heating strategy
      let progress: number;

      // When in standby (heatingStrategy is null), infer from actual temperatures
      // This ensures cooling animation reflects what was actually heated
      const effectiveStrategy = heatingStrategy ?? (steamWasHeated ? 2 : 0);

      if (!isDualBoiler || effectiveStrategy === 0) {
        // Single boiler, HX, or Brew Only strategy: only brew boiler matters
        progress = brewProgress;
      } else if (effectiveStrategy === 1) {
        // Sequential: brew heats first (0-50%), then steam (50-100%)
        // When brew is done, steam starts
        if (brewProgress < 100) {
          progress = brewProgress * 0.5; // 0-50% while brew heats
        } else {
          progress = 50 + steamProgress * 0.5; // 50-100% while steam heats
        }
      } else {
        // Parallel (2) or Smart Stagger (3): both heat together
        // Machine is ready when BOTH reach target, so use the minimum
        progress = Math.min(brewProgress, steamProgress);
      }

      const dispTemp = convertFromCelsius(brewCurrent, temperatureUnit);
      const dispSetpoint = convertFromCelsius(brewSetpoint, temperatureUnit);
      const unit = getUnitSymbol(temperatureUnit);
      return {
        heatingProgress: progress,
        displayTemp: dispTemp,
        displaySetpoint: dispSetpoint,
        unitSymbol: unit,
      };
    }, [
      brewCurrent,
      brewSetpoint,
      steamCurrent,
      steamSetpoint,
      temperatureUnit,
      isDualBoiler,
      heatingStrategy,
    ]);

  return (
    <Card className="relative overflow-hidden">
      {/* Subtle accent line at top based on state */}
      <div
        className={cn(
          "absolute top-0 left-0 right-0 h-1 transition-colors duration-500",
          state === "ready" && "bg-emerald-500",
          state === "heating" && "bg-amber-500",
          state === "brewing" && "bg-accent",
          state === "steaming" && "bg-blue-500",
          // Cooling state: standby but still hot
          mode === "standby" && heatingProgress > 10 && "bg-blue-400",
          // Fully cooled or never heated
          mode === "standby" && heatingProgress <= 10 && "bg-theme-tertiary",
          state === "idle" && mode !== "standby" && "bg-theme-tertiary"
        )}
      />

      <div className="relative space-y-6">
        <div className="flex items-center justify-between gap-4 sm:gap-6">
          {/* Status Ring - round progress to prevent flicker from tiny changes */}
          <StatusRing
            mode={mode}
            state={state}
            heatingProgress={Math.round(heatingProgress)}
          />

          {/* Status Info */}
          <div className="flex-1 min-w-0">
            <div className="flex items-center gap-3 mb-2">
              <h2 className="text-xl sm:text-2xl font-bold text-theme">
                Machine Status
              </h2>
            </div>
            <div className="flex items-center gap-2 flex-wrap">
              <Badge
                className={cn(
                  getMachineStateColor(state),
                  "text-sm px-4 py-1.5"
                )}
              >
                {getMachineStateLabel(state)}
              </Badge>
              {/* Heating strategy - show for dual boiler when machine is on */}
              {isDualBoiler &&
                mode !== "standby" &&
                heatingStrategy !== null &&
                heatingStrategy !== undefined && (
                  <span className="inline-flex items-center gap-1.5 text-xs text-theme-muted">
                    <span className="text-theme-tertiary">•</span>
                    {(() => {
                      const strategy = STRATEGY_LABELS[heatingStrategy];
                      const Icon = strategy?.icon || Flame;
                      return (
                        <>
                          <Icon className="w-3 h-3" />
                          {strategy?.label || "Unknown"}
                        </>
                      );
                    })()}
                  </span>
                )}
            </div>

            {/* Temperature display - always reserve space to prevent layout shift */}
            <div
              className={cn(
                "mt-4 flex items-center gap-2 text-theme-secondary h-7 transition-opacity duration-300",
                mode === "standby" ? "opacity-0" : "opacity-100"
              )}
            >
              <Thermometer className="w-4 h-4 text-accent flex-shrink-0" />
              <span className="text-lg font-semibold tabular-nums min-w-[4.5rem]">
                {displayTemp.toFixed(1)}
                {unitSymbol}
              </span>
              <span className="text-theme-muted text-sm flex-shrink-0">
                / {displaySetpoint.toFixed(0)}
                {unitSymbol}
              </span>
            </div>

            {/* Contextual helper text - fixed height to prevent layout shift */}
            <p className="mt-2 text-sm text-theme-muted min-h-[2.5rem]">
              {getStatusDescription(
                mode,
                state,
                heatingProgress,
                isDualBoiler,
                heatingStrategy
              )}
            </p>
          </div>
        </div>

        {/* Power Controls */}
        <PowerControls
          mode={mode}
          heatingStrategy={heatingStrategy}
          isDualBoiler={isDualBoiler}
          onSetMode={onSetMode}
          onPowerOn={onPowerOn}
        />
      </div>
    </Card>
  );
});

function getStatusDescription(
  mode: string,
  state: string,
  heatingProgress?: number,
  isDualBoiler?: boolean,
  heatingStrategy?: HeatingStrategy | null
): string {
  if (mode === "standby") {
    // Check if still cooling down
    if (heatingProgress && heatingProgress > 10) {
      return `Cooling down... ${Math.round(heatingProgress)}% residual heat.`;
    }
    return "Machine is in standby mode. Turn on to start heating.";
  }
  if (mode === "eco")
    return "Eco mode active. Lower temperature to save energy.";
  if (state === "heating") {
    if (heatingProgress && heatingProgress > 0) {
      // More descriptive message for dual boilers
      if (isDualBoiler && heatingStrategy !== 0) {
        if (heatingStrategy === 1 && heatingProgress < 50) {
          return `Heating brew boiler... ${Math.round(heatingProgress * 2)}%`;
        } else if (heatingStrategy === 1 && heatingProgress >= 50) {
          return `Heating steam boiler... ${Math.round(
            (heatingProgress - 50) * 2
          )}%`;
        }
        return `Warming up both boilers... ${Math.round(heatingProgress)}%`;
      }
      return `Warming up... ${Math.round(
        heatingProgress
      )}% to target temperature.`;
    }
    return "Warming up... Your machine will be ready soon.";
  }
  if (state === "ready") {
    if (isDualBoiler && heatingStrategy !== 0) {
      return "Both boilers at temperature. Ready to brew!";
    }
    return "Perfect temperature reached. Ready to brew!";
  }
  if (state === "brewing") return "Extraction in progress...";
  if (state === "steaming") return "Steam wand active.";
  if (state === "cooldown") return "Cooling down to target temperature.";
  return "Monitoring your espresso machine.";
}

interface StatusRingProps {
  mode: string;
  state: string;
  heatingProgress: number;
}

const StatusRing = memo(function StatusRing({
  mode,
  state,
  heatingProgress,
}: StatusRingProps) {
  const [displayProgress, setDisplayProgress] = useState(0);
  const lastUpdateRef = useRef(0);

  // Always use actual heating progress - this reflects real boiler temperature
  // Whether heating up, ready, or cooling down after turning off
  const targetProgress = heatingProgress;

  // Only update display progress when change is significant (>2%) or enough time passed
  useEffect(() => {
    const now = Date.now();
    const timeSinceLastUpdate = now - lastUpdateRef.current;
    const progressDiff = Math.abs(targetProgress - displayProgress);

    // Update if: significant change or enough time passed
    if (progressDiff > 2 || timeSinceLastUpdate > 2000) {
      lastUpdateRef.current = now;
      setDisplayProgress(targetProgress);
    }
  }, [targetProgress, displayProgress]);

  const isActive = mode !== "standby";
  const isReady = state === "ready";
  const isHeating = state === "heating";
  const isBrewing = state === "brewing" || state === "steaming";
  // Standby but still hot (cooling down)
  const isCooling = mode === "standby" && heatingProgress > 10;

  // Ring dimensions
  const size = 120;
  const strokeWidth = 8;
  const radius = (size - strokeWidth) / 2;
  const circumference = 2 * Math.PI * radius;
  const strokeDashoffset =
    circumference - (displayProgress / 100) * circumference;

  return (
    <div className="relative flex-shrink-0">
      {/* SVG Ring */}
      <svg width={size} height={size} className="transform -rotate-90">
        {/* Background ring */}
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          strokeWidth={strokeWidth}
          className="stroke-theme-tertiary"
        />

        {/* Progress ring */}
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          strokeWidth={strokeWidth}
          strokeLinecap="round"
          strokeDasharray={circumference}
          strokeDashoffset={strokeDashoffset}
          style={{ transition: "stroke-dashoffset 800ms ease-out" }}
          className={cn(
            isBrewing && "stroke-accent",
            isReady && !isBrewing && "stroke-emerald-500",
            isHeating && "stroke-amber-500",
            isCooling && "stroke-blue-400",
            !isActive && !isCooling && "stroke-theme-muted"
          )}
        />
      </svg>

      {/* Center icon */}
      <div className="absolute inset-0 flex items-center justify-center">
        <div
          className={cn(
            "w-16 h-16 rounded-full flex items-center justify-center",
            isBrewing && "bg-accent/15",
            isReady && !isBrewing && "bg-emerald-500/15",
            isHeating && "bg-amber-500/15",
            isCooling && "bg-blue-400/15",
            !isActive && !isCooling && "bg-theme-tertiary"
          )}
        >
          {isBrewing ? (
            <Coffee className="w-8 h-8 text-accent" />
          ) : (
            <Power
              className={cn(
                "w-8 h-8",
                isReady && "text-emerald-500",
                isHeating && "text-amber-500",
                isCooling && "text-blue-400",
                !isActive && !isCooling && "text-theme-muted"
              )}
            />
          )}
        </div>
      </div>
    </div>
  );
});

interface PowerControlsProps {
  mode: string;
  heatingStrategy?: HeatingStrategy | null;
  isDualBoiler?: boolean;
  onSetMode: (mode: string) => void;
  onPowerOn: () => void;
}

const PowerControls = memo(function PowerControls({
  mode,
  heatingStrategy,
  isDualBoiler,
  onSetMode,
  onPowerOn,
}: PowerControlsProps) {
  // Get the "On" button description based on heating strategy
  const getOnDescription = () => {
    if (
      mode !== "on" ||
      !isDualBoiler ||
      heatingStrategy === null ||
      heatingStrategy === undefined
    ) {
      return "Full power";
    }
    return STRATEGY_LABELS[heatingStrategy]?.label || "Full power";
  };

  const modes: {
    id: MachineMode;
    icon: typeof Power;
    label: string;
    description: string;
  }[] = [
    { id: "standby", icon: Power, label: "Standby", description: "Power off" },
    { id: "on", icon: Zap, label: "On", description: getOnDescription() },
    { id: "eco", icon: Leaf, label: "Eco", description: "Energy saving" },
  ];

  return (
    <div className="border-t border-theme pt-5">
      <div className="flex flex-col sm:flex-row gap-3">
        {modes.map((m) => {
          const Icon = m.icon;
          const isActive = mode === m.id;
          const handleClick = m.id === "on" ? onPowerOn : () => onSetMode(m.id);

          return (
            <button
              key={m.id}
              onClick={handleClick}
              className={cn(
                "group flex-1 px-5 py-4 rounded-xl font-semibold transition-colors duration-200",
                "flex items-center justify-center gap-3",
                isActive
                  ? "nav-active"
                  : "bg-theme-secondary text-theme-secondary hover:bg-theme-tertiary"
              )}
            >
              <Icon className="w-5 h-5" />
              <div className="flex flex-col items-start">
                <span className="text-sm">{m.label}</span>
                <span
                  className={cn(
                    "text-[10px] font-normal",
                    isActive ? "opacity-70" : "text-theme-muted"
                  )}
                >
                  {m.description}
                </span>
              </div>
            </button>
          );
        })}
      </div>
    </div>
  );
});
