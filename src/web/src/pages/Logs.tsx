import { useState, useEffect, useCallback } from "react";
import { useNavigate } from "react-router-dom";
import { useStore } from "@/lib/store";
import { PageHeader } from "@/components/PageHeader";
import { Button } from "@/components/Button";
import { LogViewer } from "@/components/LogViewer";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Toggle } from "@/components/Toggle";
import { Badge } from "@/components/Badge";
import {
  Trash2,
  ArrowLeft,
  Download,
  HardDrive,
  Cpu,
  RefreshCw,
  AlertTriangle,
} from "lucide-react";
import { useDevMode } from "@/lib/dev-mode";
import { formatBytes } from "@/lib/utils";

interface LogInfo {
  enabled: boolean;
  size: number;
  maxSize: number;
  picoForwarding: boolean;
}

export function Logs() {
  const navigate = useNavigate();
  const clearLogs = useStore((s) => s.clearLogs);
  const devMode = useDevMode();

  const [logInfo, setLogInfo] = useState<LogInfo | null>(null);
  const [downloading, setDownloading] = useState(false);
  const [togglingBuffer, setTogglingBuffer] = useState(false);
  const [togglingPico, setTogglingPico] = useState(false);

  // Fetch log info from device (local mode only - uses relative path)
  const fetchLogInfo = useCallback(async () => {
    try {
      const response = await fetch("/api/logs/info");
      if (response.ok) {
        const data = await response.json();
        setLogInfo(data);
      }
    } catch (error) {
      console.error("Failed to fetch log info:", error);
    }
  }, []);

  useEffect(() => {
    fetchLogInfo();
    // Refresh every 10 seconds
    const interval = setInterval(fetchLogInfo, 10000);
    return () => clearInterval(interval);
  }, [fetchLogInfo]);

  // Enable/disable log buffer
  const toggleLogBuffer = async (enabled: boolean) => {
    try {
      setTogglingBuffer(true);

      const formData = new FormData();
      formData.append("enabled", enabled.toString());

      const response = await fetch("/api/logs/enable", {
        method: "POST",
        body: formData,
      });

      if (!response.ok) {
        const error = await response.json();
        console.error("Failed to toggle log buffer:", error);
      }

      await fetchLogInfo();
    } catch (error) {
      console.error("Failed to toggle log buffer:", error);
    } finally {
      setTogglingBuffer(false);
    }
  };

  // Download logs from device
  const downloadLogs = async () => {
    try {
      setDownloading(true);

      const response = await fetch("/api/logs");
      if (response.ok) {
        const blob = await response.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = `brewos_logs_${new Date().toISOString().slice(0, 10)}.txt`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);
      }
    } catch (error) {
      console.error("Failed to download logs:", error);
    } finally {
      setDownloading(false);
    }
  };

  // Clear device logs
  const clearDeviceLogs = async () => {
    try {
      await fetch("/api/logs", { method: "DELETE" });
      await fetchLogInfo();
    } catch (error) {
      console.error("Failed to clear device logs:", error);
    }
  };

  // Toggle Pico log forwarding
  const togglePicoForwarding = async (enabled: boolean) => {
    try {
      setTogglingPico(true);

      const formData = new FormData();
      formData.append("enabled", enabled.toString());

      await fetch("/api/logs/pico", {
        method: "POST",
        body: formData,
      });

      await fetchLogInfo();
    } catch (error) {
      console.error("Failed to toggle Pico log forwarding:", error);
    } finally {
      setTogglingPico(false);
    }
  };

  const isEnabled = logInfo?.enabled ?? false;

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-4">
          <Button
            variant="ghost"
            size="sm"
            onClick={() => navigate(-1)}
            className="flex items-center gap-2"
          >
            <ArrowLeft className="w-4 h-4" />
            Back
          </Button>
          <PageHeader
            title="System Logs"
            subtitle="Debug logging (dev mode feature)"
          />
        </div>
        <div className="flex items-center gap-2">
          <Button
            variant="ghost"
            size="sm"
            onClick={fetchLogInfo}
            title="Refresh"
          >
            <RefreshCw className="w-4 h-4" />
          </Button>
          <Button variant="secondary" size="sm" onClick={clearLogs}>
            <Trash2 className="w-4 h-4" />
            Clear UI
          </Button>
        </div>
      </div>

      {/* Dev Mode Only Notice */}
      {!devMode && (
        <Card>
          <div className="flex items-center gap-3 text-amber-500">
            <AlertTriangle className="w-5 h-5" />
            <div>
              <p className="font-medium">Dev Mode Required</p>
              <p className="text-sm text-theme-muted">
                Enable dev mode in settings to access device log buffer
                features.
              </p>
            </div>
          </div>
        </Card>
      )}

      {/* Device Log Buffer Card */}
      {devMode && (
        <Card>
          <CardHeader>
            <CardTitle icon={<HardDrive className="w-5 h-5" />}>
              Device Log Buffer
            </CardTitle>
            <Badge variant={isEnabled ? "success" : "default"}>
              {isEnabled ? "Enabled" : "Disabled"}
            </Badge>
          </CardHeader>

          <div className="space-y-4">
            {/* Enable Toggle */}
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm font-medium text-theme">
                  Enable Log Buffer
                </p>
                <p className="text-xs text-theme-muted">
                  Allocates 50KB memory for capturing debug logs
                </p>
              </div>
              <Toggle
                checked={isEnabled}
                onChange={toggleLogBuffer}
                disabled={togglingBuffer}
              />
            </div>

            {isEnabled ? (
              <>
                {/* Buffer Usage */}
                <div className="flex items-center justify-between text-sm">
                  <span className="text-theme-muted">Buffer Usage</span>
                  <span className="font-mono">
                    {logInfo
                      ? `${formatBytes(logInfo.size)} / ${formatBytes(logInfo.maxSize)}`
                      : "Loading..."}
                  </span>
                </div>

                {logInfo && (
                  <div className="w-full bg-theme-secondary rounded-full h-2">
                    <div
                      className="bg-amber-500 h-2 rounded-full transition-all"
                      style={{
                        width: `${Math.min(100, (logInfo.size / logInfo.maxSize) * 100)}%`,
                      }}
                    />
                  </div>
                )}

                <p className="text-xs text-theme-muted">
                  Circular buffer - older entries automatically discarded when
                  full. Download logs before they're overwritten.
                </p>

                {/* Actions */}
                <div className="flex gap-2">
                  <Button
                    variant="primary"
                    size="sm"
                    onClick={downloadLogs}
                    disabled={downloading}
                  >
                    <Download className="w-4 h-4" />
                    {downloading ? "Downloading..." : "Download Logs"}
                  </Button>
                  <Button
                    variant="secondary"
                    size="sm"
                    onClick={clearDeviceLogs}
                  >
                    <Trash2 className="w-4 h-4" />
                    Clear Buffer
                  </Button>
                </div>
              </>
            ) : (
              <p className="text-sm text-theme-muted">
                Log buffer is disabled. Enable to start capturing ESP32 and Pico
                debug logs. When disabled, there is zero impact on memory or
                performance.
              </p>
            )}
          </div>
        </Card>
      )}

      {/* Pico Log Forwarding (only when buffer is enabled) */}
      {devMode && isEnabled && (
        <Card>
          <CardHeader>
            <CardTitle icon={<Cpu className="w-5 h-5" />}>
              Pico Log Forwarding
            </CardTitle>
          </CardHeader>

          <div className="space-y-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm font-medium text-theme">
                  Forward Pico Logs
                </p>
                <p className="text-xs text-theme-muted">
                  Stream Pico controller logs via UART to device buffer
                </p>
              </div>
              <Toggle
                checked={logInfo?.picoForwarding ?? false}
                onChange={togglePicoForwarding}
                disabled={togglingPico}
              />
            </div>

            <p className="text-xs text-theme-muted">
              Setting persists on Pico until changed. May slightly increase UART
              traffic.
            </p>
          </div>
        </Card>
      )}

      {/* Real-time WebSocket Logs */}
      <Card>
        <CardHeader>
          <CardTitle>Real-time Logs</CardTitle>
        </CardHeader>
        <p className="text-xs text-theme-muted mb-4">
          Live WebSocket log messages (operational events only - always
          available).
        </p>
        <div className="h-[calc(100vh-40rem)]">
          <LogViewer maxHeight="h-full" />
        </div>
      </Card>
    </div>
  );
}
