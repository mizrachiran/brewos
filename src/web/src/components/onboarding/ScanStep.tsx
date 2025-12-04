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
    <div className="py-2 xs:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Header */}
      <div className="text-center mb-3 xs:mb-6">
        <div className="w-10 h-10 xs:w-14 xs:h-14 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4">
          <QrCode className="w-5 h-5 xs:w-7 xs:h-7 text-accent" />
        </div>
        <h2 className="text-xl xs:text-3xl font-bold text-theme mb-1 xs:mb-2">Scan QR Code</h2>
        <p className="text-theme-muted text-xs xs:text-base max-w-md mx-auto">
          Point your camera at the QR code on your BrewOS display
        </p>
      </div>

      {/* QR Scanner */}
      <div className="mb-3 xs:mb-6">
        {onScan ? (
          <QRScanner onScan={onScan} onError={onScanError || (() => {})} />
        ) : (
          <div className="max-w-[200px] xs:max-w-xs mx-auto">
            <div className="aspect-square w-full bg-gradient-to-br from-white/5 to-white/10 xs:from-theme-secondary xs:to-theme-tertiary rounded-xl flex items-center justify-center border-2 border-dashed border-accent/30 relative overflow-hidden">
              <div className="absolute inset-0 bg-accent/5 animate-pulse" />
              <div className="relative text-center text-theme-muted z-10 px-3 xs:px-4">
                <QrCode className="w-10 h-10 xs:w-16 xs:h-16 mx-auto mb-2 xs:mb-3 opacity-40" />
                <p className="text-xs xs:text-sm font-medium">
                  Camera view will appear here
                </p>
                <p className="text-[10px] xs:text-xs mt-1 opacity-70">
                  Allow camera access when prompted
                </p>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* WiFi reminder */}
      <div className="flex items-center gap-2 text-[10px] xs:text-sm text-theme-muted justify-center mb-3 xs:mb-6 p-2 xs:p-3 bg-white/5 xs:bg-theme-secondary/50 rounded-lg xs:rounded-xl">
        <Wifi className="w-3 h-3 xs:w-4 xs:h-4 text-accent flex-shrink-0" />
        <span>Make sure your machine is connected to WiFi</span>
      </div>

      {/* Error message */}
      {error && (
        <div className="mb-3 xs:mb-4 p-2.5 xs:p-3 rounded-lg xs:rounded-xl bg-red-500/10 border border-red-500/20 animate-in fade-in slide-in-from-top-2">
          <div className="flex items-center gap-2">
            <AlertCircle className="w-3.5 h-3.5 xs:w-4 xs:h-4 text-red-500 flex-shrink-0" />
            <p className="text-[10px] xs:text-sm text-red-500">{error}</p>
          </div>
        </div>
      )}

      {/* Action buttons */}
      <div className="flex gap-2 xs:gap-3 pt-1 xs:pt-2">
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
