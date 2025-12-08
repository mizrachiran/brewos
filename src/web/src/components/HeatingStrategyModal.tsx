import { useState, useEffect, useMemo } from "react";
import { X, Flame, Wind, Zap, Brain, Check, AlertTriangle } from "lucide-react";
import { cn } from "@/lib/utils";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { getMachineByBrandModel } from "@/lib/machines";
import {
  validateHeatingStrategy,
  calculateHeaterCurrents,
  type HeatingStrategy as HeatingStrategyValue,
} from "@/lib/powerValidation";

export interface HeatingStrategy {
  value: number;
  label: string;
  desc: string;
  icon: React.ReactNode;
  detail: string;
}

export const HEATING_STRATEGIES: HeatingStrategy[] = [
  {
    value: 0,
    label: "Brew Only",
    desc: "Heat only brew boiler",
    icon: <Flame className="w-5 h-5 sm:w-6 sm:h-6" />,
    detail: "Fastest for espresso. Ideal when you only need coffee.",
  },
  {
    value: 1,
    label: "Sequential",
    desc: "Brew first, then steam",
    icon: <Wind className="w-5 h-5 sm:w-6 sm:h-6" />,
    detail: "Brew boiler heats first, then steam. Balanced and efficient.",
  },
  {
    value: 2,
    label: "Parallel",
    desc: "Heat both simultaneously",
    icon: <Zap className="w-5 h-5 sm:w-6 sm:h-6" />,
    detail: "Fastest overall. Both boilers heat at the same time.",
  },
  {
    value: 3,
    label: "Smart Stagger",
    desc: "Power-aware heating",
    icon: <Brain className="w-5 h-5 sm:w-6 sm:h-6" />,
    detail:
      "Intelligently manages power to avoid overload. Recommended for most setups.",
  },
];

interface HeatingStrategyModalProps {
  isOpen: boolean;
  onClose: () => void;
  onSelect: (strategy: number) => void;
  defaultStrategy?: number;
}

const STORAGE_KEY = "brewos-last-heating-strategy";

export function HeatingStrategyModal({
  isOpen,
  onClose,
  onSelect,
  defaultStrategy,
}: HeatingStrategyModalProps) {
  const { sendCommand } = useCommand();
  
  // Get power settings and machine info from store
  const voltage = useStore((s) => s.power.voltage) || 220;
  const maxCurrent = useStore((s) => s.power.maxCurrent) || 13;
  const machineBrand = useStore((s) => s.device.machineBrand);
  const machineModel = useStore((s) => s.device.machineModel);

  // Get machine definition by brand and model
  const machine = useMemo(() => {
    if (!machineBrand || !machineModel) return undefined;
    return getMachineByBrandModel(machineBrand, machineModel);
  }, [machineBrand, machineModel]);

  // Calculate heater currents and validate strategies
  const strategyValidations = useMemo(() => {
    const heaterCurrents = calculateHeaterCurrents(machine, voltage);
    const powerConfig = { voltage, maxCurrent };

    return HEATING_STRATEGIES.reduce((acc, strategy) => {
      acc[strategy.value] = validateHeatingStrategy(
        strategy.value as HeatingStrategyValue,
        powerConfig,
        heaterCurrents
      );
      return acc;
    }, {} as Record<number, ReturnType<typeof validateHeatingStrategy>>);
  }, [machine, voltage, maxCurrent]);

  const [selectedStrategy, setSelectedStrategy] = useState<number>(() => {
    // Get from localStorage or use default
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored !== null) {
      const parsed = parseInt(stored, 10);
      if (!isNaN(parsed) && parsed >= 0 && parsed <= 3) {
        return parsed;
      }
    }
    return defaultStrategy ?? 1; // Default to Sequential
  });

  // Update selected when default changes
  useEffect(() => {
    if (defaultStrategy !== undefined) {
      setSelectedStrategy(defaultStrategy);
    }
  }, [defaultStrategy]);

  // Handle keyboard navigation
  useEffect(() => {
    if (!isOpen) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        onClose();
      } else if (e.key.startsWith("Arrow")) {
        e.preventDefault();
        const currentIndex = HEATING_STRATEGIES.findIndex(
          (s) => s.value === selectedStrategy
        );
        let newIndex = currentIndex;

        if (e.key === "ArrowDown" || e.key === "ArrowRight") {
          newIndex = (currentIndex + 1) % HEATING_STRATEGIES.length;
        } else if (e.key === "ArrowUp" || e.key === "ArrowLeft") {
          newIndex =
            (currentIndex - 1 + HEATING_STRATEGIES.length) %
            HEATING_STRATEGIES.length;
        }

        setSelectedStrategy(HEATING_STRATEGIES[newIndex].value);
      } else if (e.key === "Enter") {
        e.preventDefault();
        // Confirm with currently selected strategy
        localStorage.setItem(STORAGE_KEY, selectedStrategy.toString());
        // Also save to ESP32 for syncing across devices
        sendCommand('set_preferences', { lastHeatingStrategy: selectedStrategy });
        onSelect(selectedStrategy);
        onClose();
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [isOpen, selectedStrategy, onClose, onSelect]);

  if (!isOpen) return null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center xs:p-4"
      onClick={onClose}
    >
      {/* Backdrop */}
      <div className="absolute inset-0 bg-black/50 xs:backdrop-blur-sm transition-opacity" />

      {/* Modal */}
      <div
        className="relative w-full h-full xs:h-auto xs:max-w-2xl xs:max-h-[90vh] bg-theme-card rounded-none xs:rounded-2xl xs:shadow-2xl border-0 xs:border border-theme overflow-hidden transform transition-all flex flex-col"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between p-4 sm:p-6 border-b border-theme flex-shrink-0">
          <div className="flex-1 min-w-0 pr-2">
            <h2 className="text-xl sm:text-2xl font-bold text-theme">
              Select Heating Strategy
            </h2>
            <p className="text-xs sm:text-sm text-theme-muted mt-1">
              Choose how the machine should heat up
            </p>
          </div>
          <button
            onClick={onClose}
            className="p-2 rounded-lg hover:bg-theme-secondary text-theme-muted hover:text-theme transition-colors flex-shrink-0"
            aria-label="Close"
          >
            <X className="w-5 h-5" />
          </button>
        </div>

        {/* Strategy Grid - Scrollable */}
        <div className="flex-1 overflow-y-auto p-4 sm:p-6">
          <div className="grid grid-cols-1 md:grid-cols-2 gap-3 sm:gap-4">
            {HEATING_STRATEGIES.map((strategy) => {
              const isSelected = selectedStrategy === strategy.value;
              const validation = strategyValidations[strategy.value];
              const isUnsafe = validation && !validation.isAllowed;

              return (
                <button
                  key={strategy.value}
                  onClick={() => {
                    if (isUnsafe) {
                      // Still allow selection but user is warned
                    }
                    setSelectedStrategy(strategy.value);
                    // Immediately confirm selection
                    localStorage.setItem(
                      STORAGE_KEY,
                      strategy.value.toString()
                    );
                    // Also save to ESP32 for syncing across devices
                    sendCommand('set_preferences', { lastHeatingStrategy: strategy.value });
                    onSelect(strategy.value);
                    onClose();
                  }}
                  className={cn(
                    "relative p-4 sm:p-5 rounded-xl border-2 transition-all text-left group",
                    "hover:scale-[1.02] hover:shadow-lg cursor-pointer active:scale-[0.98]",
                    isSelected
                      ? "border-accent bg-accent/10 shadow-md"
                      : isUnsafe
                      ? "border-amber-500/50 bg-amber-500/5 hover:border-amber-500"
                      : "border-theme bg-theme-secondary hover:border-theme-tertiary"
                  )}
                >
                  {/* Selection indicator */}
                  {isSelected && (
                    <div className="absolute top-2 right-2 sm:top-3 sm:right-3 w-5 h-5 sm:w-6 sm:h-6 rounded-full bg-accent flex items-center justify-center">
                      <Check className="w-3 h-3 sm:w-4 sm:h-4 text-white" />
                    </div>
                  )}

                  {/* Warning indicator for unsafe strategies */}
                  {isUnsafe && !isSelected && (
                    <div className="absolute top-2 right-2 sm:top-3 sm:right-3 w-5 h-5 sm:w-6 sm:h-6 rounded-full bg-amber-500/20 flex items-center justify-center">
                      <AlertTriangle className="w-3 h-3 sm:w-4 sm:h-4 text-amber-500" />
                    </div>
                  )}

                  {/* Icon */}
                  <div
                    className={cn(
                      "w-10 h-10 sm:w-12 sm:h-12 rounded-lg flex items-center justify-center mb-2 sm:mb-3 transition-colors flex-shrink-0",
                      isSelected
                        ? "bg-accent/20 text-accent"
                        : isUnsafe
                        ? "bg-amber-500/20 text-amber-500"
                        : "bg-theme-tertiary text-theme-muted group-hover:text-accent"
                    )}
                  >
                    {strategy.icon}
                  </div>

                  {/* Label */}
                  <h3
                    className={cn(
                      "text-base sm:text-lg font-semibold mb-1",
                      isSelected
                        ? "text-theme"
                        : isUnsafe
                        ? "text-amber-500"
                        : "text-theme-secondary"
                    )}
                  >
                    {strategy.label}
                  </h3>

                  {/* Description */}
                  <p className="text-xs sm:text-sm text-theme-muted mb-1.5 sm:mb-2">
                    {strategy.desc}
                  </p>

                  {/* Detail or Warning */}
                  {isUnsafe && validation?.reason ? (
                    <div className="flex items-start gap-1.5 text-xs text-amber-500">
                      <AlertTriangle className="w-3.5 h-3.5 flex-shrink-0 mt-0.5" />
                      <span>
                        May trip breaker:{" "}
                        {validation.combinedCurrent?.toFixed(1)}A needed, limit{" "}
                        {validation.safeLimit?.toFixed(1)}A
                      </span>
                    </div>
                  ) : (
                    <p className="text-xs text-theme-muted leading-relaxed">
                      {strategy.detail}
                    </p>
                  )}
                </button>
              );
            })}
          </div>

          {/* Power settings info */}
          {machine && (
            <div className="mt-4 pt-4 border-t border-theme">
              <p className="text-xs text-theme-muted text-center">
                Power settings: {voltage}V / {maxCurrent}A max
                {machine.specs.brewPowerWatts &&
                  machine.specs.steamPowerWatts && (
                    <span className="block mt-1">
                      {machine.brand} {machine.model}:{" "}
                      {machine.specs.brewPowerWatts}W brew +{" "}
                      {machine.specs.steamPowerWatts}W steam
                    </span>
                  )}
              </p>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
