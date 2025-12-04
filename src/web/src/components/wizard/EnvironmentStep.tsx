import { Zap, ChevronDown } from "lucide-react";
import { Input } from "@/components/Input";
import { cn } from "@/lib/utils";

interface EnvironmentStepProps {
  voltage: number;
  maxCurrent: number;
  errors: Record<string, string>;
  onVoltageChange: (voltage: number) => void;
  onMaxCurrentChange: (current: number) => void;
}

export function EnvironmentStep({
  voltage,
  maxCurrent,
  errors,
  onVoltageChange,
  onMaxCurrentChange,
}: EnvironmentStepProps) {
  return (
    <div className="py-6">
      <div className="text-center mb-8">
        <div className="w-16 h-16 bg-accent/10 rounded-2xl flex items-center justify-center mx-auto mb-4">
          <Zap className="w-8 h-8 text-accent" />
        </div>
        <h2 className="text-2xl font-bold text-theme mb-2">Power Settings</h2>
        <p className="text-theme-muted">
          Configure your electrical environment for safe operation
        </p>
      </div>

      <div className="space-y-4 max-w-sm mx-auto">
        <div className="space-y-2">
          <label className="block text-sm font-medium text-theme">
            Mains Voltage <span className="text-red-500">*</span>
          </label>
          <div className="relative">
            <select
              value={voltage}
              onChange={(e) => onVoltageChange(parseInt(e.target.value))}
              className={cn(
                "w-full px-4 py-3 pr-10 rounded-xl appearance-none",
                "bg-theme-secondary border border-theme",
                "text-theme text-sm",
                "focus:outline-none focus:ring-2 focus:ring-accent focus:border-transparent",
                "transition-all duration-200",
                errors.voltage && "border-red-500"
              )}
            >
              <option value="110">110V (US/Japan)</option>
              <option value="220">220V (EU/AU/Most of World)</option>
              <option value="240">240V (UK)</option>
            </select>
            <ChevronDown className="absolute right-3 top-1/2 -translate-y-1/2 w-5 h-5 text-theme-muted pointer-events-none" />
          </div>
          {errors.voltage && (
            <p className="text-xs text-red-500">{errors.voltage}</p>
          )}
        </div>

        <Input
          label="Max Circuit Current"
          type="number"
          min={10}
          max={20}
          step={1}
          value={maxCurrent}
          onChange={(e) => onMaxCurrentChange(parseInt(e.target.value) || 10)}
          unit="A"
          error={errors.maxCurrent}
          hint="Check your breaker or fuse rating"
          required
        />

        <div className="p-4 rounded-xl bg-accent/5 border border-accent/20 space-y-3">
          <div className="flex items-center gap-2">
            <Zap className="w-5 h-5 text-accent" />
            <span className="font-semibold text-theme">Common Values</span>
          </div>
          <div className="grid grid-cols-2 gap-2 text-xs text-theme-muted">
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-accent/50" />
              <span>US/CA: 110V / 15-20A</span>
            </div>
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-accent/50" />
              <span>EU: 220V / 16A</span>
            </div>
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-accent/50" />
              <span>UK/IE: 240V / 13A</span>
            </div>
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-accent/50" />
              <span>AU: 220V / 10A</span>
            </div>
          </div>
          <p className="text-xs text-theme-muted leading-relaxed">
            These settings ensure the machine heats safely within your
            electrical system's limits.
          </p>
        </div>
      </div>
    </div>
  );
}
