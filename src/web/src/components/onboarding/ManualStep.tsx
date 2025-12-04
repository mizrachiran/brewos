import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { ArrowRight } from "lucide-react";

interface ManualStepProps {
  claimCode?: string;
  deviceName?: string;
  onClaimCodeChange?: (code: string) => void;
  onDeviceNameChange?: (name: string) => void;
  error?: string;
  onBack?: () => void;
  onAdd?: () => void;
  disabled?: boolean;
  loading?: boolean;
}

export function ManualStep({
  claimCode,
  deviceName,
  onClaimCodeChange,
  onDeviceNameChange,
  error,
  onBack,
  onAdd,
  disabled = false,
  loading = false,
}: ManualStepProps) {
  return (
    <>
      <div className="text-center mb-6">
        <h2 className="text-2xl font-bold text-theme mb-2">
          Enter Pairing Code
        </h2>
        <p className="text-theme-secondary">
          Find the code on your BrewOS display under System → Cloud Access.
        </p>
      </div>

      <div className="space-y-4">
        <Input
          label="Pairing Code"
          placeholder="X6ST-AP3G"
          value={claimCode}
          onChange={(e) =>
            onClaimCodeChange?.(e.target.value.toUpperCase())
          }
          hint="Enter the 8-character code from your machine display"
        />

        <Input
          label="Machine Name (optional)"
          placeholder="Kitchen Espresso"
          value={deviceName}
          onChange={(e) => onDeviceNameChange?.(e.target.value)}
        />

        {error && (
          <p className="text-sm text-error bg-error-soft p-3 rounded-lg">
            {error}
          </p>
        )}

        <div className="flex gap-3">
          <Button variant="secondary" className="flex-1" onClick={onBack}>
            Back
          </Button>
          <Button
            className="flex-1"
            onClick={onAdd}
            disabled={disabled}
            loading={loading}
          >
            Add Machine
            <ArrowRight className="w-4 h-4" />
          </Button>
        </div>
      </div>
    </>
  );
}

