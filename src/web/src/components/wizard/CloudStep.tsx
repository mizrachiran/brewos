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
    <div className="py-3 xs:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      <div className="text-center mb-4 xs:mb-6">
        <div className="w-12 h-12 xs:w-16 xs:h-16 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4 border border-accent/20 shadow-sm">
          <Cloud className="w-6 h-6 xs:w-8 xs:h-8 text-accent" />
        </div>
        <h2 className="text-xl xs:text-3xl font-bold text-theme mb-1 xs:mb-2">Cloud Access</h2>
        <p className="text-theme-muted text-xs xs:text-base">Control your machine from anywhere</p>
      </div>

      {/* Cloud Enable/Disable Toggle */}
      <div className="bg-white/5 xs:bg-theme-secondary rounded-lg xs:rounded-xl p-3 xs:p-5 mb-4 xs:mb-6 border border-white/10 xs:border-theme/10 shadow-sm">
        <div className="flex items-center justify-between gap-3 xs:gap-4">
          <div className="flex items-center gap-2.5 xs:gap-4 flex-1">
            <div className="w-10 h-10 xs:w-12 xs:h-12 rounded-lg xs:rounded-xl bg-accent/10 flex items-center justify-center flex-shrink-0 border border-accent/20">
              {cloudEnabled ? (
                cloudConnected ? (
                  <Check className="w-5 h-5 xs:w-6 xs:h-6 text-emerald-500" />
                ) : checkingStatus ? (
                  <Loader2 className="w-5 h-5 xs:w-6 xs:h-6 animate-spin text-accent" />
                ) : (
                  <Cloud className="w-5 h-5 xs:w-6 xs:h-6 text-accent" />
                )
              ) : (
                <CloudOff className="w-5 h-5 xs:w-6 xs:h-6 text-theme-muted" />
              )}
            </div>
            <div className="flex-1 min-w-0">
              <p className="font-semibold text-theme text-sm xs:text-base mb-0.5 xs:mb-1">
                {cloudEnabled
                  ? cloudConnected
                    ? "Cloud Connected"
                    : "Cloud Integration"
                  : "Local Only Mode"}
              </p>
              <p className="text-[10px] xs:text-sm text-theme-muted leading-relaxed truncate">
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
        <div className="mb-4 xs:mb-6 p-2.5 xs:p-4 rounded-lg xs:rounded-xl bg-amber-500/10 border border-amber-500/20">
          <div className="flex items-start gap-2 xs:gap-3">
            <AlertCircle className="w-4 h-4 xs:w-5 xs:h-5 text-amber-500 flex-shrink-0 mt-0.5" />
            <div className="flex-1">
              <p className="text-xs xs:text-sm font-medium text-amber-500 mb-0.5 xs:mb-1">
                Pairing Not Complete
              </p>
              <p className="text-[10px] xs:text-xs text-theme-muted">{error}</p>
            </div>
          </div>
        </div>
      )}

      {/* Success message if cloud is connected */}
      {cloudEnabled && cloudConnected && (
        <div className="mb-4 xs:mb-6 p-2.5 xs:p-4 rounded-lg xs:rounded-xl bg-emerald-500/10 border border-emerald-500/20">
          <div className="flex items-center gap-2 xs:gap-3">
            <Check className="w-4 h-4 xs:w-5 xs:h-5 text-emerald-500 flex-shrink-0" />
            <div>
              <p className="text-xs xs:text-sm font-medium text-emerald-500">
                Cloud Successfully Connected
              </p>
              <p className="text-[10px] xs:text-xs text-theme-muted mt-0.5">
                Your machine is now accessible from cloud.brewos.io
              </p>
            </div>
          </div>
        </div>
      )}

      {cloudEnabled ? (
        <>
          {!cloudConnected && (
            <div className="bg-white/5 xs:bg-theme-secondary rounded-xl xs:rounded-2xl p-3 xs:p-6 mb-4 xs:mb-6 border border-white/10 xs:border-theme/10 shadow-sm">
              <div className="flex flex-col items-center">
                {loading ? (
                  <div className="w-32 h-32 xs:w-48 xs:h-48 flex items-center justify-center">
                    <Loader2 className="w-8 h-8 xs:w-12 xs:h-12 animate-spin text-accent" />
                  </div>
                ) : pairing ? (
                  <PairingQRCode
                    pairing={pairing}
                    copied={copied}
                    onCopy={onCopy}
                  />
                ) : (
                  <div className="text-center py-4 xs:py-8">
                    <AlertCircle className="w-8 h-8 xs:w-12 xs:h-12 text-theme-muted mx-auto mb-2 xs:mb-3 opacity-50" />
                    <p className="text-theme-muted font-medium text-xs xs:text-base">
                      Could not generate pairing code
                    </p>
                  </div>
                )}
              </div>
            </div>
          )}

          {!cloudConnected && (
            <div className="text-center">
              <button
                onClick={onSkip}
                className="text-[10px] xs:text-sm text-theme-muted hover:text-theme hover:underline"
              >
                Skip pairing for now — you can complete this later in Settings
              </button>
            </div>
          )}
        </>
      ) : (
        <div className="bg-white/5 xs:bg-theme-tertiary border border-white/10 xs:border-theme rounded-lg xs:rounded-xl p-4 xs:p-6">
          <div className="text-center space-y-3 xs:space-y-4">
            <CloudOff className="w-8 h-8 xs:w-12 xs:h-12 text-theme-muted mx-auto" />
            <div>
              <p className="font-medium text-theme text-sm xs:text-base mb-0.5 xs:mb-1">
                Operating in Local-Only Mode
              </p>
              <p className="text-xs xs:text-sm text-theme-muted max-w-xs mx-auto">
                Your machine will only be accessible on your local network at{" "}
                <span className="font-mono text-accent">brewos.local</span>
              </p>
            </div>
            <div className="pt-1 xs:pt-2">
              <p className="text-[10px] xs:text-xs text-theme-muted">
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
    <div className="w-full">
      <div className="bg-white xs:bg-theme-card p-2 xs:p-4 rounded-lg xs:rounded-xl mb-3 xs:mb-5 border border-theme/10 shadow-lg mx-auto w-fit">
        <QRCodeSVG value={pairing.url} size={120} level="M" className="xs:hidden" />
        <QRCodeSVG value={pairing.url} size={180} level="M" className="hidden xs:block" />
      </div>
      <p className="text-[10px] xs:text-sm text-theme-muted mb-3 xs:mb-4 text-center max-w-sm mx-auto leading-relaxed">
        Scan with your phone or visit{" "}
        <a
          href="https://cloud.brewos.io"
          target="_blank"
          rel="noopener noreferrer"
          className="text-accent hover:underline font-semibold"
        >
          cloud.brewos.io
        </a>
      </p>

      <div className="w-full max-w-xs mx-auto">
        <label className="block text-[10px] xs:text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5 xs:mb-2 text-center">
          Manual Code
        </label>
        <div className="flex items-center gap-1.5 xs:gap-2">
          <code className="flex-1 bg-white/10 xs:bg-theme-card px-3 xs:px-4 py-2.5 xs:py-3.5 rounded-lg xs:rounded-xl text-base xs:text-lg font-mono text-theme text-center tracking-wider border border-white/10 xs:border-theme/10 font-semibold">
            {pairing.manualCode || `${pairing.deviceId.substring(0, 8)}`}
          </code>
          <Button
            variant="secondary"
            size="sm"
            onClick={onCopy}
            className={copied ? "bg-emerald-500/10 border-emerald-500/20" : ""}
            title={copied ? "Copied!" : "Copy code"}
          >
            {copied ? (
              <Check className="w-3.5 h-3.5 xs:w-4 xs:h-4 text-emerald-500" />
            ) : (
              <Copy className="w-3.5 h-3.5 xs:w-4 xs:h-4" />
            )}
          </Button>
        </div>
        {copied && (
          <p className="text-[10px] xs:text-xs text-emerald-500 text-center mt-1.5 xs:mt-2 animate-in fade-in">
            Code copied to clipboard!
          </p>
        )}
      </div>
    </div>
  );
}
