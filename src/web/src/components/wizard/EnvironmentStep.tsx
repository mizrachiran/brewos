import { Zap, ChevronDown } from "lucide-react";
import { Input } from "@/components/Input";
import { cn } from "@/lib/utils";
import { useMobileLandscape } from "@/lib/useMobileLandscape";

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
  const isMobileLandscape = useMobileLandscape();

  const commonValues = [
    { label: "US/CA", value: "110V / 15-20A" },
    { label: "EU", value: "220V / 16A" },
    { label: "UK/IE", value: "240V / 13A" },
    { label: "AU", value: "220V / 10A" },
  ];

  // Landscape: two-column layout with larger content
  if (isMobileLandscape) {
    return (
      <div className="h-full flex flex-col animate-in fade-in duration-300">
        <div className="flex items-center gap-3 mb-4">
          <div className="w-12 h-12 bg-accent/10 rounded-xl flex items-center justify-center flex-shrink-0 border border-accent/20">
            <Zap className="w-6 h-6 text-accent" />
          </div>
          <div>
            <h2 className="text-xl font-bold text-theme">Power Settings</h2>
            <p className="text-sm text-theme-muted">
              Configure for safe operation
            </p>
          </div>
        </div>

        <div className="flex-1 flex gap-6">
          {/* Left: Inputs */}
          <div className="flex-1 space-y-4">
            <div className="space-y-2">
              <label className="block text-sm font-medium text-theme">
                Mains Voltage <span className="text-red-500">*</span>
              </label>
              <div className="relative">
                <select
                  value={voltage}
                  onChange={(e) => onVoltageChange(parseInt(e.target.value))}
                  className={cn(
                    "w-full px-4 py-3 pr-10 rounded-xl appearance-none text-sm",
                    "bg-theme-secondary border border-theme text-theme font-medium",
                    "focus:outline-none focus:ring-2 focus:ring-accent",
                    errors.voltage && "border-red-500"
                  )}
                >
                  <option value="110">110V (US/Japan)</option>
                  <option value="220">220V (EU/AU)</option>
                  <option value="240">240V (UK)</option>
                </select>
                <ChevronDown className="absolute right-3 top-1/2 -translate-y-1/2 w-5 h-5 text-theme-muted pointer-events-none" />
              </div>
            </div>
            <Input
              label="Max Circuit Current"
              type="number"
              min={10}
              max={20}
              value={maxCurrent}
              onChange={(e) =>
                onMaxCurrentChange(parseInt(e.target.value) || 10)
              }
              unit="A"
              error={errors.maxCurrent}
              hint="Check your breaker rating"
              required
            />
          </div>

          {/* Right: Common values */}
          <div className="w-64 flex-shrink-0 mx-2 mb-2">
            <div className="p-4 rounded-xl bg-accent/5 border border-accent/20 h-full">
              <div className="flex items-center gap-2 mb-2">
                <Zap className="w-5 h-5 text-accent" />
                <span className="font-semibold text-theme text-sm">
                  Common Values
                </span>
              </div>
              <div className="space-y-2">
                {commonValues.map((item, index) => (
                  <div
                    key={index}
                    className="flex justify-between items-center p-1 rounded-lg bg-theme-secondary/50 text-xs"
                  >
                    <span className="text-theme font-medium">{item.label}</span>
                    <span className="text-theme-muted text-xs">
                      {item.value}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>
    );
  }

  // Portrait: vertical layout
  return (
    <div className="py-3 xs:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      <div className="text-center mb-4 xs:mb-8">
        <div className="w-12 h-12 xs:w-16 xs:h-16 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4 border border-accent/20 shadow-sm">
          <Zap className="w-6 h-6 xs:w-8 xs:h-8 text-accent" />
        </div>
        <h2 className="text-xl xs:text-3xl font-bold text-theme mb-1 xs:mb-2">
          Power Settings
        </h2>
        <p className="text-theme-muted text-xs xs:text-base">
          Configure your electrical environment for safe operation
        </p>
      </div>

      <div className="space-y-3 xs:space-y-5 max-w-sm mx-auto">
        <div className="space-y-1.5 xs:space-y-2">
          <label className="block text-xs xs:text-sm font-medium text-theme">
            Mains Voltage <span className="text-red-500">*</span>
          </label>
          <div className="relative">
            <select
              value={voltage}
              onChange={(e) => onVoltageChange(parseInt(e.target.value))}
              className={cn(
                "w-full px-3 xs:px-4 py-2.5 xs:py-3 pr-10 rounded-lg xs:rounded-xl appearance-none",
                "bg-white/5 xs:bg-theme-secondary border border-white/10 xs:border-theme",
                "text-theme text-xs xs:text-sm font-medium",
                "focus:outline-none focus:ring-2 focus:ring-accent focus:border-transparent",
                "transition-all duration-200 hover:border-accent/50",
                errors.voltage && "border-red-500 focus:ring-red-500"
              )}
            >
              <option value="110">110V (US/Japan)</option>
              <option value="220">220V (EU/AU/Most of the World)</option>
              <option value="240">240V (UK)</option>
            </select>
            <ChevronDown className="absolute right-3 top-1/2 -translate-y-1/2 w-4 h-4 xs:w-5 xs:h-5 text-theme-muted pointer-events-none" />
          </div>
          {errors.voltage && (
            <p className="text-[10px] xs:text-xs text-red-500 animate-in fade-in slide-in-from-top-2">
              {errors.voltage}
            </p>
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

        <div className="p-3 xs:p-5 rounded-lg xs:rounded-xl bg-accent/5 border border-accent/20 space-y-2 xs:space-y-4 shadow-sm">
          <div className="flex items-center gap-2">
            <Zap className="w-4 h-4 xs:w-5 xs:h-5 text-accent" />
            <span className="font-semibold text-theme text-sm xs:text-base">
              Common Values
            </span>
          </div>
          <div className="grid grid-cols-2 gap-1.5 xs:gap-3 text-[10px] xs:text-xs">
            {[
              { label: "US/CA", value: "110V / 15-20A" },
              { label: "EU", value: "220V / 16A" },
              { label: "UK/IE", value: "240V / 13A" },
              { label: "AU", value: "220V / 10A" },
            ].map((item, index) => (
              <div
                key={index}
                className="flex items-center gap-1.5 xs:gap-2 p-1.5 xs:p-2 rounded-md xs:rounded-lg bg-white/5 xs:bg-theme-secondary/50 hover:bg-white/10 xs:hover:bg-theme-secondary transition-colors"
              >
                <span className="w-1.5 h-1.5 xs:w-2 xs:h-2 rounded-full bg-accent flex-shrink-0" />
                <div>
                  <span className="font-medium text-theme">{item.label}:</span>{" "}
                  <span className="text-theme-muted">{item.value}</span>
                </div>
              </div>
            ))}
          </div>
          <p className="text-[10px] xs:text-xs text-theme-muted leading-relaxed pt-2 border-t border-white/10 xs:border-theme/10">
            These settings ensure the machine heats safely within your
            electrical system's limits.
          </p>
        </div>
      </div>
    </div>
  );
}
