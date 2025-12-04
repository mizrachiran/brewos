import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { ArrowRight, Coffee } from "lucide-react";

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
  return (
    <div className="py-2 xs:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Header */}
      <div className="text-center mb-4 xs:mb-8">
        <div className="w-10 h-10 xs:w-14 xs:h-14 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4">
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
