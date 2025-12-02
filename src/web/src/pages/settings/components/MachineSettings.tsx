import { useState, useEffect, useMemo } from "react";
import { useStore } from "@/lib/store";
import { getActiveConnection } from "@/lib/connection";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { useToast } from "@/components/Toast";
import { Coffee, Save, ChevronDown, AlertCircle } from "lucide-react";
import {
  SUPPORTED_MACHINES,
  getMachinesGroupedByBrand,
  getMachineById,
  getMachineTypeLabel,
} from "@/lib/machines";
import { cn } from "@/lib/utils";
import { formatTemperatureWithUnit } from "@/lib/temperature";

export function MachineSettings() {
  const device = useStore((s) => s.device);
  const temperatureUnit = useStore((s) => s.preferences.temperatureUnit);
  const connectionState = useStore((s) => s.connectionState);
  const { success, error } = useToast();

  // Device identity
  const [deviceName, setDeviceName] = useState(device.deviceName);
  const [selectedMachineId, setSelectedMachineId] = useState<string>("");
  const [savingMachine, setSavingMachine] = useState(false);

  // Get grouped machines for the dropdown
  const machineGroups = useMemo(() => getMachinesGroupedByBrand(), []);

  // Find currently selected machine
  const selectedMachine = useMemo(
    () => (selectedMachineId ? getMachineById(selectedMachineId) : undefined),
    [selectedMachineId]
  );

  // Try to match existing brand/model to a supported machine
  useEffect(() => {
    setDeviceName(device.deviceName);

    // Try to find matching machine from current brand/model
    if (device.machineBrand && device.machineModel) {
      const match = SUPPORTED_MACHINES.find(
        (m) =>
          m.brand.toLowerCase() === device.machineBrand.toLowerCase() &&
          m.model.toLowerCase() === device.machineModel.toLowerCase()
      );
      if (match) {
        setSelectedMachineId(match.id);
      }
    }
  }, [device.deviceName, device.machineBrand, device.machineModel]);

  const saveMachineInfo = async () => {
    if (!deviceName.trim() || !selectedMachine) return;

    // Check if connected
    if (connectionState !== "connected") {
      error("Not connected to machine. Please wait for connection.");
      return;
    }

    setSavingMachine(true);

    try {
      const connection = getActiveConnection();
      if (!connection) {
        throw new Error("No connection available");
      }

      // Send machine info including the type
      connection.sendCommand("set_machine_info", {
        name: deviceName.trim(),
        brand: selectedMachine.brand,
        model: selectedMachine.model,
        machineType: selectedMachine.type,
        machineId: selectedMachine.id,
        // Also send default temperatures
        defaultBrewTemp: selectedMachine.defaults.brewTemp,
        defaultSteamTemp: selectedMachine.defaults.steamTemp,
      });

      // Wait a bit for command to be sent, then show success
      await new Promise((resolve) => setTimeout(resolve, 300));
      success("Machine info saved successfully");
    } catch (err) {
      console.error("Failed to save machine info:", err);
      error("Failed to save machine info. Please try again.");
    } finally {
      setSavingMachine(false);
    }
  };

  const isMachineInfoValid = deviceName.trim() && selectedMachine;
  const isMachineInfoChanged =
    deviceName !== device.deviceName ||
    (selectedMachine &&
      (selectedMachine.brand !== device.machineBrand ||
        selectedMachine.model !== device.machineModel));

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Coffee className="w-5 h-5" />}>Machine</CardTitle>
      </CardHeader>

      <div className="space-y-4">
        <Input
          label="Machine Name"
          placeholder="Kitchen Espresso"
          value={deviceName}
          onChange={(e) => setDeviceName(e.target.value)}
          hint="Give your machine a friendly name"
          required
        />

        {/* Machine Selector */}
        <div className="space-y-2">
          <label className="block text-sm font-medium text-theme">
            Machine Model <span className="text-red-500">*</span>
          </label>
          <div className="relative">
            <select
              value={selectedMachineId}
              onChange={(e) => setSelectedMachineId(e.target.value)}
              className={cn(
                "w-full px-4 py-3 pr-10 rounded-xl appearance-none",
                "bg-theme-secondary border border-theme",
                "text-theme text-sm",
                "focus:outline-none focus:ring-2 focus:ring-accent focus:border-transparent",
                "transition-all duration-200",
                !selectedMachineId && "text-theme-muted"
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
            <ChevronDown className="absolute right-3 top-1/2 -translate-y-1/2 w-5 h-5 text-theme-muted pointer-events-none" />
          </div>
          <p className="text-xs text-theme-muted">
            Select your espresso machine from the supported list
          </p>
        </div>

        {/* Selected Machine Info */}
        {selectedMachine && (
          <div className="p-4 rounded-xl bg-theme-tertiary space-y-3">
            <div className="flex items-start gap-3">
              <Coffee className="w-5 h-5 text-accent mt-0.5 shrink-0" />
              <div className="flex-1 min-w-0">
                <h4 className="font-semibold text-theme">
                  {selectedMachine.brand} {selectedMachine.model}
                </h4>
                <p className="text-sm text-theme-muted">
                  {selectedMachine.description}
                </p>
              </div>
            </div>

            <div className="grid grid-cols-2 gap-4 text-sm">
              <div>
                <span className="text-theme-muted">Type:</span>{" "}
                <span className="font-medium text-theme">
                  {getMachineTypeLabel(selectedMachine.type)}
                </span>
              </div>
              <div>
                <span className="text-theme-muted">Default Brew:</span>{" "}
                <span className="font-medium text-theme">
                  {formatTemperatureWithUnit(
                    selectedMachine.defaults.brewTemp,
                    temperatureUnit,
                    0
                  )}
                </span>
              </div>
              {selectedMachine.specs.brewPowerWatts && (
                <div>
                  <span className="text-theme-muted">Brew Power:</span>{" "}
                  <span className="font-medium text-theme">
                    {selectedMachine.specs.brewPowerWatts}W
                  </span>
                </div>
              )}
              {selectedMachine.specs.steamPowerWatts && (
                <div>
                  <span className="text-theme-muted">Steam Power:</span>{" "}
                  <span className="font-medium text-theme">
                    {selectedMachine.specs.steamPowerWatts}W
                  </span>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Machine not found notice */}
        {!selectedMachine && device.machineBrand && device.machineModel && (
          <div className="flex items-start gap-3 p-4 rounded-xl bg-amber-500/10 border border-amber-500/20">
            <AlertCircle className="w-5 h-5 text-amber-500 mt-0.5 shrink-0" />
            <div className="text-sm">
              <p className="font-medium text-amber-500">
                Current machine not in supported list
              </p>
              <p className="text-theme-muted mt-1">
                Currently configured: {device.machineBrand}{" "}
                {device.machineModel}
              </p>
              <p className="text-theme-muted">
                Please select from the dropdown to ensure proper configuration.
              </p>
            </div>
          </div>
        )}

        <div className="flex justify-end">
          <Button
            onClick={saveMachineInfo}
            loading={savingMachine}
            disabled={!isMachineInfoValid || !isMachineInfoChanged}
          >
            <Save className="w-4 h-4" />
            Save Machine Info
          </Button>
        </div>
      </div>
    </Card>
  );
}
