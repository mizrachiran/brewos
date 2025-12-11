import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { ArrowRight, Coffee } from "lucide-react";
import { useState, useEffect } from "react";

interface MachineNameStepProps {
  deviceName?: string;
  onDeviceNameChange?: (name: string) => void;
  onBack?: () => void;
  onContinue?: () => void;
  loading?: boolean;
}

export function MachineNameStep({
  deviceName,
  onDeviceNameChange,
  onBack,
  onContinue,
  loading = false,
}: MachineNameStepProps) {
  // Detect mobile landscape
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia("(orientation: landscape) and (max-height: 500px)");
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) => setIsMobileLandscape(e.matches);
    handleChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleChange);
    return () => landscapeQuery.removeEventListener("change", handleChange);
  }, []);

  // Mobile landscape: two-column layout
  if (isMobileLandscape) {
    return (
      <div className="w-full max-w-4xl px-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
        <div className="flex gap-8 items-center">
          {/* Left column: Header */}
          <div className="flex-1 flex flex-col items-center justify-center">
            <div className="w-16 h-16 bg-accent/10 rounded-2xl flex items-center justify-center mb-4">
              <Coffee className="w-8 h-8 text-accent" />
            </div>
            <h2 className="text-2xl font-bold text-theme mb-3 text-center">Name Your Machine</h2>
            <p className="text-theme-muted text-base text-center max-w-[280px] leading-relaxed">
              Give your machine a friendly name to easily identify it
            </p>
          </div>

          {/* Vertical divider */}
          <div className="w-px self-stretch min-h-[140px] bg-theme/10 flex-shrink-0" />

          {/* Right column: Input + buttons */}
          <div className="flex-1 flex flex-col items-center justify-center">
            <div className="w-full max-w-[280px] space-y-4">
              <Input
                label="Machine Name"
                placeholder="e.g., Kitchen Espresso"
                value={deviceName}
                onChange={(e) => onDeviceNameChange?.(e.target.value)}
                hint="This name will appear in your machine list"
                autoFocus
              />

              {/* Action buttons */}
              <div className="flex gap-3">
                <Button variant="secondary" className="flex-1" onClick={onBack}>
                  Back
                </Button>
                <Button
                  className="flex-1 font-semibold"
                  onClick={onContinue}
                  loading={loading}
                >
                  {loading ? "Adding..." : "Add Machine"}
                  <ArrowRight className="w-4 h-4" />
                </Button>
              </div>
            </div>
          </div>
        </div>
      </div>
    );
  }

  // Portrait layout
  return (
    <div className="py-3 xs:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Header */}
      <div className="text-center mb-4 xs:mb-8">
        <div className="w-10 h-10 xs:w-14 xs:h-14 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4 border border-accent/20 shadow-sm">
          <Coffee className="w-5 h-5 xs:w-7 xs:h-7 text-accent" />
        </div>
        <h2 className="text-xl xs:text-3xl font-bold text-theme mb-1 xs:mb-2">Name Your Machine</h2>
        <p className="text-theme-muted text-xs xs:text-base max-w-md mx-auto">
          Give your machine a friendly name to easily identify it
        </p>
      </div>

      {/* Form */}
      <div className="space-y-3 xs:space-y-6">
        <Input
          label="Machine Name"
          placeholder="e.g., Kitchen Espresso"
          value={deviceName}
          onChange={(e) => onDeviceNameChange?.(e.target.value)}
          hint="This name will appear in your machine list"
          autoFocus
        />

        {/* Action buttons */}
        <div className="flex gap-2 xs:gap-3 pt-1 xs:pt-2">
          <Button variant="secondary" className="flex-1" onClick={onBack}>
            Back
          </Button>
          <Button
            className="flex-1 font-semibold"
            onClick={onContinue}
            loading={loading}
          >
            {loading ? "Adding..." : "Add Machine"}
            <ArrowRight className="w-4 h-4" />
          </Button>
        </div>
      </div>
    </div>
  );
}
