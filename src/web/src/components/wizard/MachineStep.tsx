import { useMemo } from "react";
import { Input } from "@/components/Input";
import { Coffee, ChevronDown } from "lucide-react";
import { cn } from "@/lib/utils";
import {
  getMachinesGroupedByBrand,
  getMachineById,
  getMachineTypeLabel,
  type MachineDefinition,
} from "@/lib/machines";
import { formatTemperatureWithUnit } from "@/lib/temperature";
import { useStore } from "@/lib/store";
import { useMobileLandscape } from "@/lib/useMobileLandscape";

interface MachineStepProps {
  machineName: string;
  selectedMachineId: string;
  errors: Record<string, string>;
  onMachineNameChange: (name: string) => void;
  onMachineIdChange: (id: string) => void;
}

export function MachineStep({
  machineName,
  selectedMachineId,
  errors,
  onMachineNameChange,
  onMachineIdChange,
}: MachineStepProps) {
  const isMobileLandscape = useMobileLandscape();
  const temperatureUnit = useStore((s) => s.preferences.temperatureUnit);
  const machineGroups = useMemo(() => getMachinesGroupedByBrand(), []);
  const selectedMachine = useMemo(
    () => (selectedMachineId ? getMachineById(selectedMachineId) : undefined),
    [selectedMachineId]
  );

  // Landscape: two-column layout with larger content
  if (isMobileLandscape) {
    return (
      <div className="h-full flex flex-col animate-in fade-in duration-300">
        <div className="flex items-center gap-3 mb-4">
          <div className="w-12 h-12 bg-accent/10 rounded-xl flex items-center justify-center flex-shrink-0 border border-accent/20">
            <Coffee className="w-6 h-6 text-accent" />
          </div>
          <div>
            <h2 className="text-xl font-bold text-theme">Select Your Machine</h2>
            <p className="text-sm text-theme-muted">Choose from our supported list</p>
          </div>
        </div>

        <div className="flex-1 flex gap-6">
          {/* Left: Inputs */}
          <div className="flex-1 space-y-4">
            <Input
              label="Machine Name"
              placeholder="Kitchen Espresso"
              value={machineName}
              onChange={(e) => onMachineNameChange(e.target.value)}
              error={errors.machineName}
              hint="Give it a friendly name"
            />
            <MachineSelector
              selectedMachineId={selectedMachineId}
              machineGroups={machineGroups}
              error={errors.machineModel}
              onChange={onMachineIdChange}
            />
          </div>

          {/* Right: Machine Info */}
          <div className="flex-1">
            {selectedMachine ? (
              <MachineInfo machine={selectedMachine} temperatureUnit={temperatureUnit} />
            ) : (
              <div className="h-full flex items-center justify-center p-6 bg-theme-secondary/30 rounded-xl border border-theme/10">
                <p className="text-sm text-theme-muted text-center">
                  Select a machine to see details
                </p>
              </div>
            )}
          </div>
        </div>
      </div>
    );
  }

  // Portrait: vertical layout
  return (
    <div className="py-3 xs:py-6">
      <div className="text-center mb-4 xs:mb-8">
        <div className="w-12 h-12 xs:w-16 xs:h-16 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4">
          <Coffee className="w-6 h-6 xs:w-8 xs:h-8 text-accent" />
        </div>
        <h2 className="text-xl xs:text-2xl font-bold text-theme mb-1 xs:mb-2">Select Your Machine</h2>
        <p className="text-theme-muted text-xs xs:text-base">Choose your espresso machine from our supported list</p>
      </div>

      <div className="space-y-3 xs:space-y-4 max-w-sm mx-auto">
        <Input
          label="Machine Name"
          placeholder="Kitchen Espresso"
          value={machineName}
          onChange={(e) => onMachineNameChange(e.target.value)}
          error={errors.machineName}
          hint="Give it a friendly name"
        />

        <MachineSelector
          selectedMachineId={selectedMachineId}
          machineGroups={machineGroups}
          error={errors.machineModel}
          onChange={onMachineIdChange}
        />

        {selectedMachine && (
          <MachineInfo machine={selectedMachine} temperatureUnit={temperatureUnit} />
        )}
      </div>
    </div>
  );
}

interface MachineSelectorProps {
  selectedMachineId: string;
  machineGroups: ReturnType<typeof getMachinesGroupedByBrand>;
  error?: string;
  onChange: (id: string) => void;
  compact?: boolean;
}

function MachineSelector({
  selectedMachineId,
  machineGroups,
  error,
  onChange,
  compact = false,
}: MachineSelectorProps) {
  return (
    <div className={compact ? "space-y-1" : "space-y-1.5 xs:space-y-2"}>
      <label className={cn("block font-medium text-theme", compact ? "text-xs" : "text-xs xs:text-sm")}>
        Machine Model <span className="text-red-500">*</span>
      </label>
      <div className="relative">
        <select
          value={selectedMachineId}
          onChange={(e) => onChange(e.target.value)}
          className={cn(
            "w-full pr-10 rounded-lg appearance-none",
            "bg-theme-secondary border border-theme",
            "text-theme focus:outline-none focus:ring-2 focus:ring-accent focus:border-transparent",
            "transition-all duration-200",
            compact ? "px-3 py-2 text-xs rounded-lg" : "px-3 xs:px-4 py-2.5 xs:py-3 text-xs xs:text-sm xs:rounded-xl",
            !selectedMachineId && "text-theme-muted",
            error && "border-red-500"
          )}
        >
          <option value="">Select your machine...</option>
          {machineGroups.map((group) => (
            <optgroup key={group.brand} label={group.brand}>
              {group.machines.map((machine) => (
                <option key={machine.id} value={machine.id}>
                  {machine.model} ({getMachineTypeLabel(machine.type)})
                </option>
              ))}
            </optgroup>
          ))}
        </select>
        <ChevronDown className={cn("absolute right-3 top-1/2 -translate-y-1/2 text-theme-muted pointer-events-none", compact ? "w-4 h-4" : "w-4 h-4 xs:w-5 xs:h-5")} />
      </div>
      {error && <p className={cn("text-red-500", compact ? "text-[10px]" : "text-[10px] xs:text-xs")}>{error}</p>}
    </div>
  );
}

interface MachineInfoProps {
  machine: MachineDefinition;
  temperatureUnit: "celsius" | "fahrenheit";
  compact?: boolean;
}

function MachineInfo({ machine, temperatureUnit, compact = false }: MachineInfoProps) {
  if (compact) {
    return (
      <div className="p-3 rounded-lg bg-accent/5 border border-accent/20 space-y-2 h-full">
        <div className="flex items-center gap-2">
          <Coffee className="w-4 h-4 text-accent flex-shrink-0" />
          <span className="font-semibold text-theme text-xs truncate">
            {machine.brand} {machine.model}
          </span>
        </div>
        <p className="text-[10px] text-theme-muted line-clamp-2">{machine.description}</p>
        <div className="flex flex-wrap gap-1 text-[9px]">
          <span className="px-1.5 py-0.5 rounded-full bg-theme-secondary text-theme-muted">
            {getMachineTypeLabel(machine.type)}
          </span>
          <span className="px-1.5 py-0.5 rounded-full bg-theme-secondary text-theme-muted">
            {formatTemperatureWithUnit(machine.defaults.brewTemp, temperatureUnit, 0)}
          </span>
        </div>
      </div>
    );
  }

  return (
    <div className="p-3 xs:p-4 rounded-lg xs:rounded-xl bg-accent/5 border border-accent/20 space-y-2 xs:space-y-3">
      <div className="flex items-center gap-2">
        <Coffee className="w-4 h-4 xs:w-5 xs:h-5 text-accent" />
        <span className="font-semibold text-theme text-sm xs:text-base">
          {machine.brand} {machine.model}
        </span>
      </div>
      <p className="text-xs xs:text-sm text-theme-muted">{machine.description}</p>
      
      {/* Type and Temperatures */}
      <div className="flex flex-wrap gap-1.5 xs:gap-2 text-[10px] xs:text-xs">
        <span className="px-1.5 xs:px-2 py-0.5 xs:py-1 rounded-full bg-white/10 xs:bg-theme-secondary text-theme-muted">
          {getMachineTypeLabel(machine.type)}
        </span>
        <span className="px-1.5 xs:px-2 py-0.5 xs:py-1 rounded-full bg-white/10 xs:bg-theme-secondary text-theme-muted">
          Brew: {formatTemperatureWithUnit(machine.defaults.brewTemp, temperatureUnit, 0)}
        </span>
        <span className="px-1.5 xs:px-2 py-0.5 xs:py-1 rounded-full bg-white/10 xs:bg-theme-secondary text-theme-muted">
          Steam: {formatTemperatureWithUnit(machine.defaults.steamTemp, temperatureUnit, 0)}
        </span>
      </div>

      {/* Power and Capacity Specs - hidden on mobile for compactness */}
      {(machine.specs.brewPowerWatts || machine.specs.steamPowerWatts || machine.specs.boilerVolumeMl) && (
        <div className="hidden xs:flex flex-wrap gap-2 text-xs">
          {machine.specs.brewPowerWatts && (
            <span className="px-2 py-1 rounded-full bg-theme-tertiary text-theme-muted">
              Brew: {machine.specs.brewPowerWatts}W
            </span>
          )}
          {machine.specs.steamPowerWatts && (
            <span className="px-2 py-1 rounded-full bg-theme-tertiary text-theme-muted">
              Steam: {machine.specs.steamPowerWatts}W
            </span>
          )}
          {machine.specs.boilerVolumeMl && (
            <span className="px-2 py-1 rounded-full bg-theme-tertiary text-theme-muted">
              Boiler: {machine.specs.boilerVolumeMl}ml
            </span>
          )}
        </div>
      )}
    </div>
  );
}

