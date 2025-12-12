import { useState, useEffect, useCallback } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Button } from "@/components/Button";
import { Badge } from "@/components/Badge";
import { LogViewer } from "@/components/LogViewer";
import { formatBytes } from "@/lib/utils";
import {
  Cpu,
  HardDrive,
  Download,
  Terminal,
  Trash2,
  AlertTriangle,
  RefreshCw,
  Check,
  FlaskConical,
  Shield,
  ExternalLink,
  Info,
  Maximize2,
} from "lucide-react";
import { StatusRow } from "./StatusRow";
import {
  checkForUpdates,
  getUpdateChannel,
  setUpdateChannel,
  getVersionDisplay,
  formatReleaseDate,
  type UpdateChannel,
  type UpdateCheckResult,
} from "@/lib/updates";
import { useDevMode } from "@/lib/dev-mode";
import { useNavigate } from "react-router-dom";
import { useBackendInfo } from "@/lib/backend-info";

export function SystemSettings() {
  const navigate = useNavigate();
  const esp32 = useStore((s) => s.esp32);
  const pico = useStore((s) => s.pico);
  const connectionState = useStore((s) => s.connectionState);
  const clearLogs = useStore((s) => s.clearLogs);
  const { sendCommandWithConfirm } = useCommand();
  
  const isConnected = connectionState === "connected";
  const devMode = useDevMode();
  const backendInfo = useBackendInfo((s) => s.info);

  const [checkingUpdate, setCheckingUpdate] = useState(false);
  const [updateError, setUpdateError] = useState<string | null>(null);
  const [updateResult, setUpdateResult] = useState<UpdateCheckResult | null>(
    null
  );
  const [channel, setChannel] = useState<UpdateChannel>(() =>
    getUpdateChannel()
  );
  const [showBetaWarning, setShowBetaWarning] = useState(false);

  const handleCheckForUpdates = useCallback(async () => {
    if (!esp32.version) {
      setUpdateError("Device version not available. Please connect to the device and try again.");
      return;
    }
    const version = esp32.version;

    setCheckingUpdate(true);
    setUpdateError(null);
    try {
      const result = await checkForUpdates(version);
      setUpdateResult(result);
    } catch (error) {
      console.error("Failed to check for updates:", error);
      setUpdateError("Failed to check for updates. Please try again.");
    } finally {
      setCheckingUpdate(false);
    }
  }, [esp32.version]);

  // Check for updates on mount or when channel changes
  useEffect(() => {
    // Auto-check for updates when component mounts
    handleCheckForUpdates();
  }, [channel, handleCheckForUpdates]);

  const [showDevWarning, setShowDevWarning] = useState(false);

  const handleChannelChange = (newChannel: UpdateChannel) => {
    if (newChannel === "beta" && channel === "stable") {
      setShowBetaWarning(true);
    } else if (newChannel === "dev" && channel !== "dev") {
      setShowDevWarning(true);
    } else {
      setChannel(newChannel);
      setUpdateChannel(newChannel);
    }
  };

  const confirmBetaChannel = () => {
    setChannel("beta");
    setUpdateChannel("beta");
    setShowBetaWarning(false);
  };

  const confirmDevChannel = () => {
    setChannel("dev");
    setUpdateChannel("dev");
    setShowDevWarning(false);
  };

  const startOTA = async (version: string) => {
    const isDev = version === "dev-latest";
    const isBeta = version.includes("-") && !isDev;

    const title = isDev
      ? "Install Dev Build?"
      : isBeta
      ? `Install Beta v${version}?`
      : `Install v${version}?`;

    const description = isDev
      ? "This is an automated build from main branch for developers. May be unstable. The device will restart after update."
      : isBeta
      ? "This is a pre-release version for testing. The device will restart after update."
      : "The device will restart after the update is installed.";

    await sendCommandWithConfirm(
      "ota_start",
      description,
      { version },
      {
        title,
        variant: isDev ? "danger" : isBeta ? "warning" : "info",
        confirmText: "Install",
        successMessage: `Installing v${version}...`,
      }
    );
  };

  const currentVersionDisplay = getVersionDisplay(esp32.version || "0.0.0");

  const restartDevice = async () => {
    await sendCommandWithConfirm(
      "restart",
      "The device will reboot. This only takes a few seconds.",
      undefined,
      {
        title: "Restart Device?",
        variant: "warning",
        confirmText: "Restart",
        successMessage: "Restarting device...",
      }
    );
  };

  const factoryReset = async () => {
    await sendCommandWithConfirm(
      "factory_reset",
      "This will erase ALL settings including WiFi credentials, machine configuration, schedules, statistics, and cloud pairing. The device will restart in setup mode. You'll need to reconnect to the BrewOS-Setup WiFi network to reconfigure. This action cannot be undone.",
      undefined,
      {
        title: "Factory Reset?",
        variant: "danger",
        confirmText: "Reset Everything",
        successMessage: "Factory reset initiated. Device is restarting in setup mode...",
      }
    );
  };

  return (
    <div className="space-y-6">
      {/* Device Info */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <Card>
          <CardHeader>
            <CardTitle icon={<Cpu className="w-5 h-5" />}>Network Controller</CardTitle>
            <Badge variant={isConnected ? "success" : "error"}>
              {isConnected ? "Online" : "Offline"}
            </Badge>
          </CardHeader>
          <div className="space-y-3">
            <StatusRow
              label="Firmware"
              value={esp32.version || "Unknown"}
              mono
            />
            {channel === "dev" && backendInfo?.buildDate && (
              <StatusRow
                label="Build"
                value={backendInfo.buildDate}
                mono
              />
            )}
            <StatusRow label="Free Heap" value={formatBytes(esp32.freeHeap)} />
          </div>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle icon={<HardDrive className="w-5 h-5" />}>
              Machine Controller
            </CardTitle>
            <Badge variant={pico.connected ? "success" : "error"}>
              {pico.connected ? "Connected" : "Disconnected"}
            </Badge>
          </CardHeader>
          <div className="space-y-3">
            <StatusRow
              label="Firmware"
              value={pico.version || "Unknown"}
              mono
            />
            {channel === "dev" && backendInfo?.picoBuildDate && (
              <StatusRow
                label="Build"
                value={backendInfo.picoBuildDate}
                mono
              />
            )}
            <StatusRow
              label="Status"
              value={pico.connected ? "Connected" : "No response"}
            />
          </div>
        </Card>
      </div>

      {/* Firmware Update */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Download className="w-5 h-5" />}>
            Firmware Update
          </CardTitle>
        </CardHeader>

        {/* Current Version */}
        <div className="flex items-center gap-3 mb-6 p-4 bg-theme rounded-xl">
          <div className="flex-1">
            <p className="text-xs text-theme-muted uppercase tracking-wider mb-1">
              Installed Version
            </p>
            <div className="flex items-center gap-2">
              <span className="font-mono text-lg font-bold text-theme">
                {esp32.version || "Unknown"}
              </span>
              {currentVersionDisplay.badge && (
                <Badge
                  variant={
                    currentVersionDisplay.badge === "stable"
                      ? "success"
                      : "warning"
                  }
                >
                  {currentVersionDisplay.badge === "stable" ? (
                    <>
                      <Shield className="w-3 h-3" /> Official
                    </>
                  ) : (
                    <>
                      <FlaskConical className="w-3 h-3" />{" "}
                      {currentVersionDisplay.badge.toUpperCase()}
                    </>
                  )}
                </Badge>
              )}
            </div>
          </div>
          <Button
            variant="secondary"
            size="sm"
            onClick={handleCheckForUpdates}
            disabled={checkingUpdate}
          >
            <RefreshCw
              className={`w-4 h-4 ${checkingUpdate ? "animate-spin" : ""}`}
            />
            {checkingUpdate ? "Checking..." : "Check"}
          </Button>
        </div>

        {/* Update Channel Selection */}
        <div className="mb-6">
          <label className="text-sm font-medium text-theme mb-3 block">
            Update Channel{" "}
            {devMode && (
              <span className="text-purple-400 text-xs">(Dev Mode)</span>
            )}
          </label>
          <div
            className={`grid gap-3 ${devMode ? "grid-cols-3" : "grid-cols-2"}`}
          >
            <button
              onClick={() => handleChannelChange("stable")}
              className={`p-4 rounded-xl border-2 transition-all text-left ${
                channel === "stable"
                  ? "border-emerald-500 bg-emerald-500/10"
                  : "border-theme hover:border-theme-light"
              }`}
            >
              <div className="flex items-center gap-2 mb-1">
                <Shield
                  className={`w-5 h-5 ${
                    channel === "stable"
                      ? "text-emerald-500"
                      : "text-theme-muted"
                  }`}
                />
                <span
                  className={`font-semibold ${
                    channel === "stable" ? "text-success" : "text-theme"
                  }`}
                >
                  Stable
                </span>
              </div>
              <p className="text-xs text-theme-muted">
                Recommended. Tested and reliable.
              </p>
            </button>

            <button
              onClick={() => handleChannelChange("beta")}
              className={`p-4 rounded-xl border-2 transition-all text-left ${
                channel === "beta"
                  ? "border-amber-500 bg-amber-500/10"
                  : "border-theme hover:border-theme-light"
              }`}
            >
              <div className="flex items-center gap-2 mb-1">
                <FlaskConical
                  className={`w-5 h-5 ${
                    channel === "beta" ? "text-amber-500" : "text-theme-muted"
                  }`}
                />
                <span
                  className={`font-semibold ${
                    channel === "beta" ? "text-warning" : "text-theme"
                  }`}
                >
                  Beta
                </span>
              </div>
              <p className="text-xs text-theme-muted">
                New features early. May have bugs.
              </p>
            </button>

            {/* Dev channel - only visible in dev mode */}
            {devMode && (
              <button
                onClick={() => handleChannelChange("dev")}
                className={`p-4 rounded-xl border-2 transition-all text-left ${
                  channel === "dev"
                    ? "border-purple-500 bg-purple-500/10"
                    : "border-theme hover:border-theme-light"
                }`}
              >
                <div className="flex items-center gap-2 mb-1">
                  <Terminal
                    className={`w-5 h-5 ${
                      channel === "dev" ? "text-purple-500" : "text-theme-muted"
                    }`}
                  />
                  <span
                    className={`font-semibold ${
                      channel === "dev" ? "text-purple-400" : "text-theme"
                    }`}
                  >
                    Dev
                  </span>
                </div>
                <p className="text-xs text-theme-muted">
                  Latest from main. For developers.
                </p>
              </button>
            )}
          </div>
        </div>

        {/* Update Check Error */}
        {updateError && (
          <div className="p-4 rounded-xl border border-error bg-error-soft mb-4">
            <div className="flex items-center gap-2 text-error">
              <AlertTriangle className="w-4 h-4" />
              <span className="text-sm">{updateError}</span>
            </div>
          </div>
        )}

        {/* No Releases Found */}
        {updateResult && !updateResult.stable && !updateResult.beta && !updateResult.dev && (
          <div className="p-4 rounded-xl border border-theme bg-theme-secondary mb-4">
            <div className="flex items-center gap-2 text-theme-muted">
              <Info className="w-4 h-4" />
              <span className="text-sm">No releases found. Check your internet connection or try again later.</span>
            </div>
          </div>
        )}

        {/* Available Updates */}
        {updateResult && (
          <div className="space-y-4">
            {/* Stable Update */}
            {updateResult.stable && (
              <div
                className={`p-4 rounded-xl border ${
                  updateResult.hasStableUpdate
                    ? "border-success bg-success-soft"
                    : "border-theme bg-theme-secondary"
                }`}
              >
                <div className="flex items-start justify-between gap-4">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <Shield className="w-4 h-4 text-emerald-500" />
                      <span className="font-semibold text-theme">
                        Official Release
                      </span>
                      <Badge variant="success">
                        v{updateResult.stable.version}
                      </Badge>
                    </div>
                    <p className="text-xs text-theme-muted mb-2">
                      Released{" "}
                      {formatReleaseDate(updateResult.stable.releaseDate)}
                    </p>
                    {updateResult.hasStableUpdate ? (
                      <p className="text-sm text-success flex items-center gap-1">
                        <Check className="w-4 h-4" />
                        Update available
                      </p>
                    ) : (
                      <p className="text-sm text-theme-muted">
                        You're on the latest stable version
                      </p>
                    )}
                  </div>
                  <div className="flex flex-col gap-2">
                    {/* Show Install if update available OR in dev mode (for testing OTA) */}
                    {(updateResult.hasStableUpdate || devMode) && (
                      <Button
                        size="sm"
                        onClick={() => startOTA(updateResult.stable!.version)}
                        variant={updateResult.hasStableUpdate ? "primary" : "secondary"}
                      >
                        <Download className="w-4 h-4" />
                        {updateResult.hasStableUpdate ? "Install" : "Reinstall"}
                      </Button>
                    )}
                    {updateResult.stable.downloadUrl && (
                      <a
                        href={updateResult.stable.downloadUrl}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-accent hover:underline flex items-center gap-1"
                      >
                        Release Notes <ExternalLink className="w-3 h-3" />
                      </a>
                    )}
                  </div>
                </div>
              </div>
            )}

            {/* Beta Update - only show if user is on beta channel */}
            {channel === "beta" && updateResult.beta && (
              <div
                className={`p-4 rounded-xl border ${
                  updateResult.hasBetaUpdate
                    ? "border-warning bg-warning-soft"
                    : "border-theme bg-theme-secondary"
                }`}
              >
                <div className="flex items-start justify-between gap-4">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <FlaskConical className="w-4 h-4 text-amber-500" />
                      <span className="font-semibold text-theme">
                        Beta Release
                      </span>
                      <Badge variant="warning">
                        v{updateResult.beta.version}
                      </Badge>
                    </div>
                    <p className="text-xs text-theme-muted mb-2">
                      Released{" "}
                      {formatReleaseDate(updateResult.beta.releaseDate)}
                    </p>
                    {updateResult.hasBetaUpdate ? (
                      <p className="text-sm text-warning flex items-center gap-1">
                        <Check className="w-4 h-4" />
                        New beta available
                      </p>
                    ) : (
                      <p className="text-sm text-theme-muted">
                        You're on the latest beta version
                      </p>
                    )}

                    {/* Beta Warning */}
                    <div className="mt-3 p-2 bg-warning-soft rounded-lg flex items-start gap-2">
                      <AlertTriangle className="w-4 h-4 text-warning flex-shrink-0 mt-0.5" />
                      <p className="text-xs text-warning">
                        Beta versions are for testing. They may contain bugs or
                        incomplete features.
                      </p>
                    </div>
                  </div>
                  <div className="flex flex-col gap-2">
                    {/* Show Install if update available OR in dev mode (for testing OTA) */}
                    {(updateResult.hasBetaUpdate || devMode) && (
                      <Button
                        size="sm"
                        variant="secondary"
                        onClick={() => startOTA(updateResult.beta!.version)}
                      >
                        <Download className="w-4 h-4" />
                        {updateResult.hasBetaUpdate ? "Install Beta" : "Reinstall Beta"}
                      </Button>
                    )}
                    {updateResult.beta.downloadUrl && (
                      <a
                        href={updateResult.beta.downloadUrl}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-accent hover:underline flex items-center gap-1"
                      >
                        Release Notes <ExternalLink className="w-3 h-3" />
                      </a>
                    )}
                  </div>
                </div>
              </div>
            )}

            {/* Dev Build - only show if user is on dev channel AND dev mode enabled */}
            {devMode && channel === "dev" && updateResult.dev && (
              <div className="p-4 rounded-xl border border-purple-500 bg-purple-500/10">
                <div className="flex items-start justify-between gap-4">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <Terminal className="w-4 h-4 text-purple-500" />
                      <span className="font-semibold text-theme">
                        Dev Build
                      </span>
                      <Badge className="bg-purple-500/20 text-purple-400 border-purple-500/30">
                        {updateResult.dev.version}
                      </Badge>
                    </div>
                    <p className="text-xs text-theme-muted mb-2">
                      Built {formatReleaseDate(updateResult.dev.releaseDate)}
                    </p>
                    <p className="text-sm text-purple-400 flex items-center gap-1">
                      <RefreshCw className="w-4 h-4" />
                      Latest from main branch
                    </p>

                    {/* Dev Warning */}
                    <div className="mt-3 p-2 bg-purple-500/10 rounded-lg flex items-start gap-2">
                      <AlertTriangle className="w-4 h-4 text-purple-400 flex-shrink-0 mt-0.5" />
                      <p className="text-xs text-purple-300">
                        Dev builds are for development testing only. They may be
                        unstable or contain breaking changes.
                      </p>
                    </div>
                  </div>
                  <div className="flex flex-col gap-2">
                    <Button
                      size="sm"
                      className="bg-purple-600 hover:bg-purple-700"
                      onClick={() => startOTA("dev-latest")}
                    >
                      <Download className="w-4 h-4" />
                      Install Dev
                    </Button>
                    {updateResult.dev.downloadUrl && (
                      <a
                        href={updateResult.dev.downloadUrl}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-purple-400 hover:underline flex items-center gap-1"
                      >
                        View Build <ExternalLink className="w-3 h-3" />
                      </a>
                    )}
                  </div>
                </div>
              </div>
            )}

            {/* No updates available message */}
            {!updateResult.hasStableUpdate &&
              (channel !== "beta" || !updateResult.hasBetaUpdate) &&
              (channel !== "dev" || !updateResult.hasDevUpdate) && (
                <div className="text-center py-4 text-theme-muted">
                  <Check className="w-8 h-8 mx-auto mb-2 text-emerald-500" />
                  <p className="text-sm">You're running the latest version!</p>
                </div>
              )}
          </div>
        )}
      </Card>

      {/* Beta Warning Modal */}
      {showBetaWarning && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 xs:backdrop-blur-sm xs:p-4">
          <Card className="w-full h-full xs:h-auto xs:max-w-md rounded-none xs:rounded-2xl flex flex-col">
            <div className="text-center p-6 flex-1 xs:flex-initial flex flex-col justify-center">
              <div className="w-16 h-16 bg-amber-500/20 rounded-full flex items-center justify-center mx-auto mb-4">
                <FlaskConical className="w-8 h-8 text-amber-500" />
              </div>
              <h3 className="text-xl font-bold text-theme mb-2">
                Enable Beta Updates?
              </h3>
              <p className="text-sm text-theme-muted mb-4">
                Beta versions include new features before they're officially
                released. They may contain bugs or incomplete features.
              </p>
              <div className="p-3 bg-amber-500/10 border border-amber-500/20 rounded-lg mb-6 text-left">
                <h4 className="font-semibold text-amber-500 text-sm mb-2 flex items-center gap-2">
                  <Info className="w-4 h-4" /> What to expect:
                </h4>
                <ul className="text-xs text-theme-muted space-y-1">
                  <li>• Early access to new features</li>
                  <li>• Potential bugs or stability issues</li>
                  <li>• More frequent updates</li>
                  <li>• You can switch back to stable anytime</li>
                </ul>
              </div>
              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  className="flex-1"
                  onClick={() => setShowBetaWarning(false)}
                >
                  Cancel
                </Button>
                <Button className="flex-1" onClick={confirmBetaChannel}>
                  <FlaskConical className="w-4 h-4" />
                  Enable Beta
                </Button>
              </div>
            </div>
          </Card>
        </div>
      )}

      {/* Dev Warning Modal - only in dev mode */}
      {devMode && showDevWarning && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 xs:backdrop-blur-sm xs:p-4">
          <Card className="w-full h-full xs:h-auto xs:max-w-md rounded-none xs:rounded-2xl flex flex-col">
            <div className="text-center p-6 flex-1 xs:flex-initial flex flex-col justify-center">
              <div className="w-16 h-16 bg-purple-500/20 rounded-full flex items-center justify-center mx-auto mb-4">
                <Terminal className="w-8 h-8 text-purple-500" />
              </div>
              <h3 className="text-xl font-bold text-theme mb-2">
                Enable Dev Builds?
              </h3>
              <p className="text-sm text-theme-muted mb-4">
                Dev builds are automated builds from the main branch. They're
                intended for developers testing the latest changes.
              </p>
              <div className="p-3 bg-purple-500/10 border border-purple-500/20 rounded-lg mb-6 text-left">
                <h4 className="font-semibold text-purple-400 text-sm mb-2 flex items-center gap-2">
                  <AlertTriangle className="w-4 h-4" /> Warning:
                </h4>
                <ul className="text-xs text-theme-muted space-y-1">
                  <li>• Untested, may contain breaking changes</li>
                  <li>• Could be unstable or crash</li>
                  <li>• Auto-updates on every push to main</li>
                  <li>• For development testing only</li>
                </ul>
              </div>
              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  className="flex-1"
                  onClick={() => setShowDevWarning(false)}
                >
                  Cancel
                </Button>
                <Button
                  className="flex-1 bg-purple-600 hover:bg-purple-700"
                  onClick={confirmDevChannel}
                >
                  <Terminal className="w-4 h-4" />
                  Enable Dev
                </Button>
              </div>
            </div>
          </Card>
        </div>
      )}

      {/* Logs */}
      <Card>
        <CardHeader
          action={
            <div className="flex items-center gap-2">
              <Button
                variant="ghost"
                size="sm"
                onClick={() => navigate("/logs")}
                title="View full screen"
              >
                <Maximize2 className="w-4 h-4" />
              </Button>
              <Button variant="ghost" size="sm" onClick={clearLogs}>
                <Trash2 className="w-4 h-4" />
                Clear
              </Button>
            </div>
          }
        >
          <CardTitle icon={<Terminal className="w-5 h-5" />}>
            System Logs
          </CardTitle>
        </CardHeader>

        <LogViewer />
      </Card>

      {/* Danger Zone */}
      <Card>
        <CardHeader>
          <CardTitle icon={<AlertTriangle className="w-5 h-5 text-red-500" />}>
            <span className="text-red-600">Danger Zone</span>
          </CardTitle>
        </CardHeader>

        <div className="flex flex-col sm:flex-row gap-4">
          <div className="flex-1">
            <h4 className="font-semibold text-theme mb-1">Restart Device</h4>
            <p className="text-sm text-theme-muted">
              Reboot the network controller. Settings will be preserved.
            </p>
          </div>
          <Button variant="secondary" onClick={restartDevice}>
            <RefreshCw className="w-4 h-4" />
            Restart
          </Button>
        </div>

        <hr className="my-4 border-theme" />

        <div className="flex flex-col sm:flex-row gap-4">
          <div className="flex-1">
            <h4 className="font-semibold text-red-600 mb-1">Factory Reset</h4>
            <p className="text-sm text-theme-muted">
              Erase all settings and return to factory defaults. This cannot be
              undone.
            </p>
          </div>
          <Button variant="danger" onClick={factoryReset}>
            <Trash2 className="w-4 h-4" />
            Factory Reset
          </Button>
        </div>
      </Card>
    </div>
  );
}
