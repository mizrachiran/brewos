import { useState, useEffect, useCallback } from "react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { useAppStore } from "@/lib/mode";
import { isDemoMode } from "@/lib/demo-mode";
import {
  Copy,
  RefreshCw,
  Check,
  Loader2,
  Share2,
  X,
  Users,
} from "lucide-react";
import { QRCodeSVG } from "qrcode.react";

interface ShareData {
  deviceId: string;
  token: string;
  url: string;
  manualCode: string;
  expiresAt: string;
  expiresIn: number;
}

// Demo mode mock data
const DEMO_SHARE: ShareData = {
  deviceId: "DEMO-ECM-001",
  token: "DEMO1234ABCD5678",
  url: "https://cloud.brewos.io/pair?id=DEMO-ECM-001&token=DEMO1234ABCD5678&share=true",
  manualCode: "DEMO-1234",
  expiresAt: new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString(),
  expiresIn: 24 * 60 * 60,
};

interface ShareDeviceProps {
  deviceId: string;
  deviceName: string;
  onClose: () => void;
}

export function ShareDevice({
  deviceId,
  deviceName,
  onClose,
}: ShareDeviceProps) {
  const { getAccessToken } = useAppStore();
  const isDemo = isDemoMode();

  const [shareData, setShareData] = useState<ShareData | null>(
    isDemo ? DEMO_SHARE : null
  );
  const [loading, setLoading] = useState(!isDemo);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);
  const [copiedCode, setCopiedCode] = useState(false);
  const [timeLeft, setTimeLeft] = useState(DEMO_SHARE.expiresIn);

  const generateShareLink = useCallback(async () => {
    if (isDemo) {
      setShareData({ ...DEMO_SHARE, expiresIn: 24 * 60 * 60 });
      setTimeLeft(24 * 60 * 60);
      return;
    }

    setLoading(true);
    setError(null);

    try {
      const token = await getAccessToken();
      if (!token) {
        setError("Not authenticated");
        setLoading(false);
        return;
      }

      const response = await fetch(`/api/devices/${deviceId}/share`, {
        method: "POST",
        headers: {
          Authorization: `Bearer ${token}`,
          "Content-Type": "application/json",
        },
      });

      if (!response.ok) {
        const data = await response.json();
        throw new Error(data.error || "Failed to generate share link");
      }

      const data = await response.json();
      setShareData(data);
      setTimeLeft(data.expiresIn);
    } catch (err) {
      const message =
        err instanceof Error ? err.message : "Failed to generate share link";
      setError(message);
    } finally {
      setLoading(false);
    }
  }, [deviceId, getAccessToken, isDemo]);

  // Generate share link on mount
  useEffect(() => {
    if (!isDemo) {
      generateShareLink();
    }
  }, [generateShareLink, isDemo]);

  // Countdown timer
  useEffect(() => {
    if (!shareData || timeLeft <= 0) return;

    const timer = setInterval(() => {
      setTimeLeft((prev) => {
        if (prev <= 1) {
          clearInterval(timer);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(timer);
  }, [shareData, timeLeft]);

  // Close on ESC key
  useEffect(() => {
    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", handleEscape);
    return () => window.removeEventListener("keydown", handleEscape);
  }, [onClose]);

  const copyShareUrl = () => {
    if (shareData?.url) {
      navigator.clipboard.writeText(shareData.url);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const copyShareCode = () => {
    if (shareData?.manualCode) {
      navigator.clipboard.writeText(shareData.manualCode);
      setCopiedCode(true);
      setTimeout(() => setCopiedCode(false), 2000);
    }
  };

  const formatTimeLeft = (seconds: number) => {
    const hours = Math.floor(seconds / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    if (hours > 0) {
      return `${hours}h ${mins}m`;
    }
    return `${mins}m`;
  };

  const isExpired = timeLeft <= 0;

  return (
    <div
      className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50 animate-in fade-in duration-200"
      onClick={(e) => {
        if (e.target === e.currentTarget) {
          onClose();
        }
      }}
    >
      <Card className="w-full max-w-md max-h-[90vh] overflow-y-auto animate-in zoom-in-95 duration-200 shadow-2xl">
        {/* Header */}
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <div className="p-2 bg-accent/10 rounded-lg">
              <Share2 className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-semibold text-theme">Share Device</h2>
              <p className="text-sm text-theme-muted truncate max-w-[200px]">
                {deviceName}
              </p>
            </div>
          </div>
          <button
            onClick={onClose}
            className="w-8 h-8 rounded-lg hover:bg-theme-secondary flex items-center justify-center transition-colors"
            title="Close"
          >
            <X className="w-4 h-4 text-theme-muted" />
          </button>
        </div>

        {/* Content */}
        {loading ? (
          <div className="flex flex-col items-center justify-center py-12">
            <Loader2 className="w-8 h-8 text-accent animate-spin mb-3" />
            <p className="text-sm text-theme-muted">Generating share link...</p>
          </div>
        ) : error ? (
          <div className="flex flex-col items-center justify-center py-8 text-center">
            <div className="w-12 h-12 rounded-full bg-error-soft flex items-center justify-center mb-3">
              <X className="w-6 h-6 text-error" />
            </div>
            <p className="font-medium text-theme mb-1">
              Failed to generate link
            </p>
            <p className="text-sm text-theme-muted mb-4">{error}</p>
            <Button variant="secondary" size="sm" onClick={generateShareLink}>
              Try Again
            </Button>
          </div>
        ) : shareData ? (
          <>
            {/* Info banner */}
            <div className="bg-theme-secondary/50 rounded-xl p-3 mb-4">
              <div className="flex items-start gap-2">
                <Users className="w-4 h-4 text-accent mt-0.5 flex-shrink-0" />
                <p className="text-xs text-theme-muted">
                  Share this link with others to give them access to your
                  device. They'll need a BrewOS account to add it.
                </p>
              </div>
            </div>

            {/* QR Code */}
            <div className="bg-white rounded-xl p-4 border border-theme flex flex-col items-center mb-4">
              <div
                className={`p-2 bg-white rounded-lg ${
                  isExpired ? "opacity-50" : ""
                }`}
              >
                <QRCodeSVG
                  value={shareData.url}
                  size={160}
                  level="M"
                  includeMargin={false}
                />
              </div>
              {isExpired ? (
                <p className="text-xs text-error mt-2 font-medium">
                  Link expired
                </p>
              ) : (
                <p className="text-xs text-theme-muted mt-2">
                  Expires in {formatTimeLeft(timeLeft)}
                </p>
              )}
            </div>

            {/* Actions */}
            <div className="mt-4">
              <Button
                variant="secondary"
                className="w-full"
                onClick={generateShareLink}
                disabled={loading}
              >
                <RefreshCw
                  className={`w-4 h-4 mr-2 ${loading ? "animate-spin" : ""}`}
                />
                New Link
              </Button>
            </div>

            {/* Share Link & Code */}
            {shareData && (
              <div className="mt-4 pt-4 border-t border-theme space-y-4">
                <div>
                  <h3 className="text-sm font-medium text-theme mb-2">
                    Share Link
                  </h3>
                  <div className="flex items-center gap-2">
                    <code className="flex-1 bg-theme-secondary px-3 py-2.5 rounded-lg text-xs font-mono text-theme truncate">
                      {shareData.url}
                    </code>
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={copyShareUrl}
                      disabled={isExpired}
                    >
                      {copied ? (
                        <Check className="w-4 h-4" />
                      ) : (
                        <Copy className="w-4 h-4" />
                      )}
                    </Button>
                  </div>
                </div>

                <div>
                  <h3 className="text-sm font-medium text-theme mb-2">
                    Manual Code
                  </h3>
                  <div className="flex items-center gap-2">
                    <code className="flex-1 bg-theme-secondary px-4 py-3 rounded-lg text-lg font-mono text-theme tracking-wider text-center">
                      {shareData.manualCode}
                    </code>
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={copyShareCode}
                      disabled={isExpired}
                    >
                      {copiedCode ? (
                        <Check className="w-4 h-4" />
                      ) : (
                        <Copy className="w-4 h-4" />
                      )}
                    </Button>
                  </div>
                  <p className="text-xs text-theme-muted mt-2">
                    Others can enter this at cloud.brewos.io to add your device
                  </p>
                </div>
              </div>
            )}
          </>
        ) : null}
      </Card>
    </div>
  );
}
