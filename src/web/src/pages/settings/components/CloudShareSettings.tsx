import { useState, useEffect, useCallback } from "react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Badge } from "@/components/Badge";
import { useAppStore } from "@/lib/mode";
import { isDemoMode } from "@/lib/demo-mode";
import {
  Share2,
  Copy,
  RefreshCw,
  Check,
  Loader2,
  Cloud,
  Wifi,
  Shield,
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

/**
 * Cloud settings for cloud mode - shows device sharing and connection status
 */
export function CloudShareSettings() {
  const { getAccessToken, getSelectedDevice } = useAppStore();
  const isDemo = isDemoMode();
  const selectedDevice = getSelectedDevice();

  const [shareData, setShareData] = useState<ShareData | null>(
    isDemo ? DEMO_SHARE : null
  );
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);
  const [copiedCode, setCopiedCode] = useState(false);
  const [timeLeft, setTimeLeft] = useState(isDemo ? DEMO_SHARE.expiresIn : 0);

  const generateShareLink = useCallback(async () => {
    if (!selectedDevice && !isDemo) {
      setError("No device selected");
      return;
    }

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

      const response = await fetch(`/api/devices/${selectedDevice!.id}/share`, {
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
  }, [selectedDevice, getAccessToken, isDemo]);

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
  const deviceId = isDemo ? "DEMO-ECM-001" : selectedDevice?.id;
  const deviceName = isDemo ? "Demo Machine" : selectedDevice?.name;

  return (
    <div className="space-y-6">
      <div className="grid gap-6 md:grid-cols-2 items-stretch">
        {/* Share Device Card */}
        <Card className="h-full">
          <div className="flex items-center gap-3 mb-4">
            <div className="p-2 bg-accent/10 rounded-lg">
              <Share2 className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-semibold text-theme">Share Device</h2>
              <p className="text-sm text-theme-muted">
                Let others access your machine
              </p>
            </div>
          </div>

          {/* Info */}
          <div className="bg-theme-secondary/50 rounded-xl p-3 mb-4">
            <div className="flex items-start gap-2">
              <Users className="w-4 h-4 text-accent mt-0.5 flex-shrink-0" />
              <p className="text-xs text-theme-muted">
                Generate a share link to give others access to your device.
                They'll need a BrewOS account to add it.
              </p>
            </div>
          </div>

          {/* QR Code or Generate Button */}
          <div className="bg-white rounded-xl p-4 border border-theme flex flex-col items-center mb-4">
            {loading ? (
              <div className="w-40 h-40 flex items-center justify-center">
                <Loader2 className="w-8 h-8 animate-spin text-accent" />
              </div>
            ) : error ? (
              <div className="w-40 h-40 flex flex-col items-center justify-center text-center">
                <p className="text-sm text-theme-muted mb-3">{error}</p>
                <Button variant="secondary" size="sm" onClick={generateShareLink}>
                  Try Again
                </Button>
              </div>
            ) : shareData ? (
              <>
                <div
                  className={`p-2 bg-white rounded-lg ${isExpired ? "opacity-50" : ""}`}
                >
                  <QRCodeSVG
                    value={shareData.url}
                    size={140}
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
              </>
            ) : (
              <div className="w-40 h-40 flex flex-col items-center justify-center">
                <Button onClick={generateShareLink}>Generate Share Link</Button>
              </div>
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
              {shareData ? "New Link" : "Generate Link"}
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
        </Card>

        {/* Cloud Status Card */}
        <Card className="h-full">
          <div className="flex items-center gap-3 mb-4">
            <div className="p-2 bg-accent/10 rounded-lg">
              <Cloud className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-semibold text-theme">Cloud Status</h2>
              <p className="text-sm text-theme-muted">
                Your connection to BrewOS Cloud
              </p>
            </div>
          </div>

          <div className="space-y-3">
            <div className="flex items-center justify-between py-2">
              <div className="flex items-center gap-2">
                <Wifi className="w-4 h-4 text-theme-muted" />
                <span className="text-sm text-theme-secondary">
                  Cloud Connection
                </span>
              </div>
              <Badge variant="success">
                <Check className="w-3 h-3 mr-1" />
                Connected
              </Badge>
            </div>

            <div className="flex items-center justify-between py-2">
              <div className="flex items-center gap-2">
                <Shield className="w-4 h-4 text-theme-muted" />
                <span className="text-sm text-theme-secondary">Device</span>
              </div>
              <span className="text-sm text-theme-muted font-mono truncate max-w-[150px]">
                {deviceName || "—"}
              </span>
            </div>

            <div className="flex items-center justify-between py-2">
              <div className="flex items-center gap-2">
                <Shield className="w-4 h-4 text-theme-muted" />
                <span className="text-sm text-theme-secondary">Machine ID</span>
              </div>
              <span className="text-sm text-theme-muted font-mono">
                {deviceId || "—"}
              </span>
            </div>

            {selectedDevice && (
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Wifi className="w-4 h-4 text-theme-muted" />
                  <span className="text-sm text-theme-secondary">
                    Device Status
                  </span>
                </div>
                <Badge variant={selectedDevice.isOnline ? "success" : "default"}>
                  {selectedDevice.isOnline ? "Online" : "Offline"}
                </Badge>
              </div>
            )}
          </div>

          {/* Help text */}
          <div className="mt-6 pt-4 border-t border-theme">
            <p className="text-xs text-theme-muted">
              You're connected to your machine through BrewOS Cloud. To change
              cloud settings on your device, connect directly to your machine's
              local network.
            </p>
          </div>
        </Card>
      </div>
    </div>
  );
}

