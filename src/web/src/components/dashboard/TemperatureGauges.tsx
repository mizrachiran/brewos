import { memo } from "react";
import { Gauge } from "@/components/Gauge";
import { Flame, Wind } from "lucide-react";

interface TemperatureData {
  current: number;
  setpoint: number;
  max: number;
}

interface TemperatureGaugesProps {
  machineType: string | undefined;
  brewTemp: TemperatureData;
  steamTemp: TemperatureData;
  groupTemp: number;
}

export const TemperatureGauges = memo(function TemperatureGauges({
  machineType,
  brewTemp,
  steamTemp,
  groupTemp,
}: TemperatureGaugesProps) {
  return (
    <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
      {/* Dual Boiler: Brew + Steam */}
      {machineType === "dual_boiler" && (
        <>
          <Gauge
            value={brewTemp.current}
            max={brewTemp.max}
            setpoint={brewTemp.setpoint}
            label="Brew Boiler"
            icon={<Flame className="w-5 h-5" />}
            variant="default"
          />
          <Gauge
            value={steamTemp.current}
            max={steamTemp.max}
            setpoint={steamTemp.setpoint}
            label="Steam Boiler"
            icon={<Wind className="w-5 h-5" />}
            variant="steam"
          />
        </>
      )}

      {/* Single Boiler: One boiler gauge */}
      {machineType === "single_boiler" && (
        <Gauge
          value={brewTemp.current}
          max={brewTemp.max}
          setpoint={brewTemp.setpoint}
          label="Boiler"
          icon={<Flame className="w-5 h-5" />}
          variant="default"
        />
      )}

      {/* Heat Exchanger: Steam Boiler + Group Head */}
      {machineType === "heat_exchanger" && (
        <>
          <Gauge
            value={steamTemp.current}
            max={steamTemp.max}
            setpoint={steamTemp.setpoint}
            label="Steam Boiler"
            icon={<Wind className="w-5 h-5" />}
            variant="steam"
          />
          <Gauge
            value={groupTemp}
            max={105}
            setpoint={93}
            label="Group Head"
            icon={<Flame className="w-5 h-5" />}
            variant="default"
          />
        </>
      )}

      {/* Unknown machine type - show both as fallback */}
      {!machineType && (
        <>
          <Gauge
            value={brewTemp.current}
            max={brewTemp.max}
            setpoint={brewTemp.setpoint}
            label="Brew Boiler"
            icon={<Flame className="w-5 h-5" />}
            variant="default"
          />
          <Gauge
            value={steamTemp.current}
            max={steamTemp.max}
            setpoint={steamTemp.setpoint}
            label="Steam Boiler"
            icon={<Wind className="w-5 h-5" />}
            variant="steam"
          />
        </>
      )}
    </div>
  );
});

