import { Button } from "@/components/Button";
import { Toggle } from "@/components/Toggle";
import {
  Cloud,
  Loader2,
  Check,
  Copy,
  CloudOff,
  AlertCircle,
} from "lucide-react";
import { QRCodeSVG } from "qrcode.react";
import type { PairingData } from "./types";

interface CloudStepProps {
  pairing: PairingData | null;
  loading: boolean;
  copied: boolean;
  cloudEnabled: boolean;
  cloudConnected?: boolean;
  checkingStatus?: boolean;
  error?: string;
  onCopy: () => void;
  onSkip: () => void;
  onCloudEnabledChange: (enabled: boolean) => void;
}

export function CloudStep({
  pairing,
  loading,
  copied,
  cloudEnabled,
  cloudConnected = false,
  checkingStatus = false,
  error,
  onCopy,
  onSkip,
  onCloudEnabledChange,
}: CloudStepProps) {
  return (
    <div className="py-6">
      <div className="text-center mb-6">
        <div className="w-16 h-16 bg-accent/10 rounded-2xl flex items-center justify-center mx-auto mb-4">
          <Cloud className="w-8 h-8 text-accent" />
        </div>
        <h2 className="text-2xl font-bold text-theme mb-2">Cloud Access</h2>
        <p className="text-theme-muted">Control your machine from anywhere</p>
      </div>

      {/* Cloud Enable/Disable Toggle */}
      <div className="bg-theme-secondary rounded-xl p-4 mb-6">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            {cloudEnabled ? (
              cloudConnected ? (
                <Check className="w-5 h-5 text-emerald-500" />
              ) : checkingStatus ? (
                <Loader2 className="w-5 h-5 animate-spin text-accent" />
              ) : (
                <Cloud className="w-5 h-5 text-accent" />
              )
            ) : (
              <CloudOff className="w-5 h-5 text-theme-muted" />
            )}
            <div>
              <p className="font-medium text-theme">
                {cloudEnabled
                  ? cloudConnected
                    ? "Cloud Connected"
                    : "Cloud Integration"
                  : "Local Only Mode"}
              </p>
              <p className="text-sm text-theme-muted">
                {cloudEnabled
                  ? cloudConnected
                    ? "Successfully paired with cloud"
                    : checkingStatus
                    ? "Checking connection status..."
                    : "Connect via cloud.brewos.io"
                  : "Access only on your local network"}
              </p>
            </div>
          </div>
          <Toggle checked={cloudEnabled} onChange={onCloudEnabledChange} />
        </div>
      </div>

      {/* Warning if cloud enabled but not connected */}
      {cloudEnabled && !cloudConnected && !checkingStatus && error && (
        <div className="mb-6 p-4 rounded-xl bg-amber-500/10 border border-amber-500/20">
          <div className="flex items-start gap-3">
            <AlertCircle className="w-5 h-5 text-amber-500 flex-shrink-0 mt-0.5" />
            <div className="flex-1">
              <p className="text-sm font-medium text-amber-500 mb-1">
                Pairing Not Complete
              </p>
              <p className="text-xs text-theme-muted">{error}</p>
            </div>
          </div>
        </div>
      )}

      {/* Success message if cloud is connected */}
      {cloudEnabled && cloudConnected && (
        <div className="mb-6 p-4 rounded-xl bg-emerald-500/10 border border-emerald-500/20">
          <div className="flex items-center gap-3">
            <Check className="w-5 h-5 text-emerald-500 flex-shrink-0" />
            <div>
              <p className="text-sm font-medium text-emerald-500">
                Cloud Successfully Connected
              </p>
              <p className="text-xs text-theme-muted mt-0.5">
                Your machine is now accessible from cloud.brewos.io
              </p>
            </div>
          </div>
        </div>
      )}

      {cloudEnabled ? (
        <>
          {!cloudConnected && (
            <div className="bg-theme-secondary rounded-2xl p-6 mb-6">
              <div className="flex flex-col items-center">
                {loading ? (
                  <div className="w-48 h-48 flex items-center justify-center">
                    <Loader2 className="w-8 h-8 animate-spin text-accent" />
                  </div>
                ) : pairing ? (
                  <PairingQRCode
                    pairing={pairing}
                    copied={copied}
                    onCopy={onCopy}
                  />
                ) : (
                  <p className="text-theme-muted">
                    Could not generate pairing code
                  </p>
                )}
              </div>
            </div>
          )}

          {!cloudConnected && (
            <div className="text-center">
              <button
                onClick={onSkip}
                className="text-sm text-theme-muted hover:text-theme hover:underline"
              >
                Skip pairing for now — you can complete this later in Settings
              </button>
            </div>
          )}
        </>
      ) : (
        <div className="bg-theme-tertiary border border-theme rounded-xl p-6">
          <div className="text-center space-y-4">
            <CloudOff className="w-12 h-12 text-theme-muted mx-auto" />
            <div>
              <p className="font-medium text-theme mb-1">
                Operating in Local-Only Mode
              </p>
              <p className="text-sm text-theme-muted max-w-xs mx-auto">
                Your machine will only be accessible on your local network at{" "}
                <span className="font-mono text-accent">brewos.local</span>
              </p>
            </div>
            <div className="pt-2">
              <p className="text-xs text-theme-muted">
                You can enable cloud access anytime in Settings → Cloud
              </p>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

interface PairingQRCodeProps {
  pairing: PairingData;
  copied: boolean;
  onCopy: () => void;
}

function PairingQRCode({ pairing, copied, onCopy }: PairingQRCodeProps) {
  return (
    <>
      <div className="bg-theme-card p-3 rounded-xl mb-4">
        <QRCodeSVG value={pairing.url} size={160} level="M" />
      </div>
      <p className="text-sm text-theme-muted mb-3">
        Scan with your phone or visit{" "}
        <a
          href="https://cloud.brewos.io"
          target="_blank"
          rel="noopener noreferrer"
          className="text-accent hover:underline"
        >
          cloud.brewos.io
        </a>
      </p>

      <div className="w-full max-w-xs">
        <div className="flex items-center gap-2">
          <code className="flex-1 bg-theme-card px-4 py-3 rounded-lg text-lg font-mono text-theme text-center tracking-wider">
            {pairing.manualCode || `${pairing.deviceId.substring(0, 8)}`}
          </code>
          <Button variant="secondary" size="sm" onClick={onCopy}>
            {copied ? (
              <Check className="w-4 h-4" />
            ) : (
              <Copy className="w-4 h-4" />
            )}
          </Button>
        </div>
      </div>
    </>
  );
}
