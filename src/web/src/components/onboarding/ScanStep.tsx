import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { QRScanner } from "@/components/QRScanner";
import { ArrowRight, Wifi, QrCode } from "lucide-react";

interface ScanStepProps {
  deviceName?: string;
  onDeviceNameChange?: (name: string) => void;
  onScan?: (result: string) => void;
  onScanError?: (error: string) => void;
  error?: string;
  onBack?: () => void;
  onAdd?: () => void;
  disabled?: boolean;
  loading?: boolean;
}

export function ScanStep({
  deviceName,
  onDeviceNameChange,
  onScan,
  onScanError,
  error,
  onBack,
  onAdd,
  disabled = false,
  loading = false,
}: ScanStepProps) {
  return (
    <>
      <div className="text-center mb-4">
        <h2 className="text-2xl font-bold text-theme mb-2">Scan QR Code</h2>
        <p className="text-theme-muted">
          Find the QR code on your BrewOS display or web interface.
        </p>
      </div>

      {/* QR Scanner */}
      <div className="mb-4">
        {onScan ? (
          <QRScanner
            onScan={onScan}
            onError={onScanError || (() => {})}
          />
        ) : (
          <div className="aspect-square max-w-xs mx-auto bg-black/20 rounded-2xl flex items-center justify-center border-2 border-dashed border-accent/50">
            <div className="text-center text-theme-muted">
              <QrCode className="w-16 h-16 mx-auto mb-2 opacity-50" />
              <p className="text-sm">Camera view would appear here</p>
            </div>
          </div>
        )}
      </div>

      <div className="flex items-center gap-2 text-xs text-theme-muted justify-center mb-4">
        <Wifi className="w-4 h-4" />
        <span>Make sure your machine is connected to WiFi</span>
      </div>

      <div className="border-t border-theme pt-4 space-y-4">
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

