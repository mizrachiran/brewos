import { Button } from "@/components/Button";
import { Logo } from "@/components/Logo";
import { QrCode } from "lucide-react";

interface WelcomeStepProps {
  onScanClick?: () => void;
  onManualClick?: () => void;
}

export function WelcomeStep({ onScanClick, onManualClick }: WelcomeStepProps) {
  return (
    <div className="py-8">
      <div className="flex justify-center mb-6">
        <Logo size="lg" />
      </div>

      <h1 className="text-3xl font-bold text-theme mb-2">
        Welcome to BrewOS
      </h1>
      <p className="text-theme-muted mb-8 max-w-sm mx-auto">
        Control your espresso machine from anywhere. Let's add your first
        machine.
      </p>

      <div className="space-y-4">
        <Button className="w-full py-4 text-lg" onClick={onScanClick}>
          <QrCode className="w-5 h-5" />
          Scan QR Code
        </Button>

        <p className="text-sm text-theme-muted">
          or{" "}
          <button
            className="text-accent hover:underline"
            onClick={onManualClick}
          >
            enter code manually
          </button>
        </p>
      </div>
    </div>
  );
}

