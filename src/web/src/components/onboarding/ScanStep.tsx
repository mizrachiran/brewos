import { Button } from "@/components/Button";
import { QRScanner } from "@/components/QRScanner";
import { ArrowRight, Wifi, QrCode, AlertCircle } from "lucide-react";

interface ScanStepProps {
  onScan?: (result: string) => void;
  onScanError?: (error: string) => void;
  error?: string;
  onBack?: () => void;
  onValidate?: () => void;
  disabled?: boolean;
  loading?: boolean;
}

export function ScanStep({
  onScan,
  onScanError,
  error,
  onBack,
  onValidate,
  disabled = false,
  loading = false,
}: ScanStepProps) {
  return (
    <div className="py-4 sm:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Header */}
      <div className="text-center mb-4 sm:mb-6">
        <div className="w-12 h-12 sm:w-14 sm:h-14 bg-accent/10 rounded-xl sm:rounded-2xl flex items-center justify-center mx-auto mb-3 sm:mb-4">
          <QrCode className="w-6 h-6 sm:w-7 sm:h-7 text-accent" />
        </div>
        <h2 className="text-2xl sm:text-3xl font-bold text-theme mb-2">Scan QR Code</h2>
        <p className="text-theme-muted text-sm sm:text-base max-w-md mx-auto">
          Point your camera at the QR code on your BrewOS display
        </p>
      </div>

      {/* QR Scanner */}
      <div className="mb-4 sm:mb-6">
        {onScan ? (
          <QRScanner onScan={onScan} onError={onScanError || (() => {})} />
        ) : (
          <div className="max-w-xs mx-auto">
            <div className="aspect-square w-full bg-gradient-to-br from-theme-secondary to-theme-tertiary rounded-xl flex items-center justify-center border-2 border-dashed border-accent/30 relative overflow-hidden">
              <div className="absolute inset-0 bg-accent/5 animate-pulse" />
              <div className="relative text-center text-theme-muted z-10 px-4">
                <QrCode className="w-14 h-14 sm:w-16 sm:h-16 mx-auto mb-3 opacity-40" />
                <p className="text-sm font-medium">
                  Camera view will appear here
                </p>
                <p className="text-xs mt-1 opacity-70">
                  Allow camera access when prompted
                </p>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* WiFi reminder */}
      <div className="flex items-center gap-2 text-xs sm:text-sm text-theme-muted justify-center mb-4 sm:mb-6 p-2.5 sm:p-3 bg-theme-secondary/50 rounded-xl">
        <Wifi className="w-3.5 h-3.5 sm:w-4 sm:h-4 text-accent" />
        <span>Make sure your machine is connected to WiFi</span>
      </div>

      {/* Error message */}
      {error && (
        <div className="mb-4 p-3 rounded-xl bg-red-500/10 border border-red-500/20 animate-in fade-in slide-in-from-top-2">
          <div className="flex items-center gap-2">
            <AlertCircle className="w-4 h-4 text-red-500 flex-shrink-0" />
            <p className="text-xs sm:text-sm text-red-500">{error}</p>
          </div>
        </div>
      )}

      {/* Action buttons */}
      <div className="flex gap-3 pt-2">
        {onBack && (
          <Button variant="secondary" className="flex-1" onClick={onBack}>
            Back
          </Button>
        )}
        <Button
          className={onBack ? "flex-1 font-semibold" : "w-full font-semibold"}
          onClick={onValidate}
          disabled={disabled}
          loading={loading}
        >
          {loading ? "Validating..." : "Continue"}
          <ArrowRight className="w-4 h-4" />
        </Button>
      </div>
    </div>
  );
}
