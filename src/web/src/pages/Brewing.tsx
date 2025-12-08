import { useState, useEffect } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Toggle } from "@/components/Toggle";
import { Badge } from "@/components/Badge";
import { PageHeader } from "@/components/PageHeader";
import { Coffee, Scale, Timer, Droplet, Waves } from "lucide-react";
import { formatDuration } from "@/lib/utils";

export function Brewing() {
  const bbw = useStore((s) => s.bbw);
  const shot = useStore((s) => s.shot);
  const scale = useStore((s) => s.scale);
  const preinfusion = useStore((s) => s.preinfusion);
  const { sendCommand } = useCommand();

  // Local form state for BBW
  const [formState, setFormState] = useState(bbw);
  const [shotTime, setShotTime] = useState(0);
  const [saving, setSaving] = useState(false);

  // Local form state for pre-infusion
  const [preinfusionForm, setPreinfusionForm] = useState(preinfusion);
  const [savingPreinfusion, setSavingPreinfusion] = useState(false);

  // Sync with store
  useEffect(() => {
    setFormState(bbw);
  }, [bbw]);

  // Sync pre-infusion with store
  useEffect(() => {
    setPreinfusionForm(preinfusion);
  }, [preinfusion]);

  // Auto-disable BBW when scale disconnects
  useEffect(() => {
    if (!scale.connected && formState.enabled) {
      setFormState((prev) => ({ ...prev, enabled: false }));
      // Also send to backend to ensure consistency
      sendCommand("set_bbw", { ...formState, enabled: false });
    }
  }, [scale.connected]); // eslint-disable-line react-hooks/exhaustive-deps

  // Shot timer
  useEffect(() => {
    let interval: ReturnType<typeof setInterval>;
    if (shot.active) {
      interval = setInterval(() => {
        setShotTime(Date.now() - shot.startTime);
      }, 100);
    }
    return () => clearInterval(interval);
  }, [shot.active, shot.startTime]);

  const ratio =
    formState.doseWeight > 0
      ? (formState.targetWeight / formState.doseWeight).toFixed(1)
      : "0.0";

  const saveSettings = () => {
    setSaving(true);
    sendCommand(
      "set_bbw",
      { ...formState },
      { successMessage: "Brewing settings saved" }
    );
    setSaving(false);
  };

  const savePreinfusion = () => {
    setSavingPreinfusion(true);
    sendCommand(
      "set_preinfusion",
      {
        enabled: preinfusionForm.enabled,
        onTimeMs: preinfusionForm.onTimeMs,
        pauseTimeMs: preinfusionForm.pauseTimeMs,
      },
      { successMessage: "Pre-infusion settings saved" }
    );
    setSavingPreinfusion(false);
  };

  const tareScale = () => {
    sendCommand("tare");
  };

  const resetScale = () => {
    sendCommand("scale_reset");
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <PageHeader
        title="Brewing"
        subtitle="Configure pre-infusion and brew-by-weight settings"
      />

      {/* Active Shot */}
      {shot.active && (
        <Card className="bg-gradient-to-br from-accent/10 to-accent/5 border-accent/30">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-xl font-bold text-theme">Active Shot</h2>
            <span className="text-3xl font-mono font-bold text-theme">
              {formatDuration(shotTime)}
            </span>
          </div>

          <div className="grid grid-cols-2 gap-6 mb-4">
            <div className="text-center">
              <div className="text-4xl font-bold text-theme tabular-nums">
                {shot.weight.toFixed(1)}
              </div>
              <div className="text-sm text-theme-muted">grams</div>
            </div>
            <div className="text-center">
              <div className="text-4xl font-bold text-theme tabular-nums">
                {shot.flowRate.toFixed(1)}
              </div>
              <div className="text-sm text-theme-muted">g/s flow</div>
            </div>
          </div>

          <div className="relative h-3 bg-theme-secondary rounded-full overflow-hidden">
            <div
              className="absolute inset-y-0 left-0 rounded-full bg-gradient-to-r from-amber-400 to-accent transition-all duration-200"
              style={{
                width: `${Math.min(
                  100,
                  (shot.weight / bbw.targetWeight) * 100
                )}%`,
              }}
            />
          </div>
          <p className="text-sm text-theme-muted mt-2">
            Target: {bbw.targetWeight}g
          </p>
        </Card>
      )}

      {/* Scale Status - First, as it's the foundation for BBW */}
      <Card
        className={
          scale.connected
            ? "bg-gradient-to-br from-emerald-500/5 to-emerald-500/0 border-emerald-500/20"
            : ""
        }
      >
        <CardHeader
          action={
            <Badge variant={scale.connected ? "success" : "error"}>
              {scale.connected ? (
                <span className="flex items-center gap-1.5">
                  <span className="relative flex h-2 w-2">
                    <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                    <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-500"></span>
                  </span>
                  Connected
                </span>
              ) : (
                "Not connected"
              )}
            </Badge>
          }
        >
          <CardTitle icon={<Scale className="w-5 h-5" />}>Scale</CardTitle>
        </CardHeader>

        {scale.connected ? (
          <>
            {/* Scale Info Bar */}
            <div className="flex flex-wrap items-center gap-3 mb-4 pb-4 border-b border-theme">
              <div className="flex items-center gap-2">
                <span className="text-sm font-medium text-theme">
                  {scale.name || "BLE Scale"}
                </span>
                {scale.type && (
                  <Badge variant="info" className="text-xs">
                    {scale.type}
                  </Badge>
                )}
              </div>
              <div className="flex items-center gap-3 ml-auto">
                {scale.battery > 0 && (
                  <div className="flex items-center gap-1 text-sm text-theme-muted">
                    <svg
                      className="w-4 h-4"
                      fill="none"
                      viewBox="0 0 24 24"
                      stroke="currentColor"
                    >
                      <path
                        strokeLinecap="round"
                        strokeLinejoin="round"
                        strokeWidth={2}
                        d="M3 7h14a2 2 0 012 2v6a2 2 0 01-2 2H3a2 2 0 01-2-2V9a2 2 0 012-2zm18 3v4"
                      />
                      <rect
                        x="4"
                        y="9"
                        width={Math.max(2, (scale.battery / 100) * 10)}
                        height="6"
                        rx="1"
                        fill="currentColor"
                      />
                    </svg>
                    <span>{scale.battery}%</span>
                  </div>
                )}
                <Badge
                  variant={scale.stable ? "success" : "warning"}
                  className="text-xs"
                >
                  {scale.stable ? "● Stable" : "◐ Settling"}
                </Badge>
              </div>
            </div>

            {/* Weight & Flow */}
            <div className="grid grid-cols-2 gap-4 mb-6">
              <div className="text-center p-4 bg-emerald-500/10 rounded-xl border border-emerald-500/20">
                <Droplet className="w-6 h-6 text-emerald-600 dark:text-emerald-400 mx-auto mb-2" />
                <div className="text-3xl font-bold text-theme tabular-nums">
                  {scale.weight.toFixed(1)}
                </div>
                <div className="text-sm text-theme-muted">grams</div>
              </div>
              <div className="text-center p-4 bg-emerald-500/10 rounded-xl border border-emerald-500/20">
                <Timer className="w-6 h-6 text-emerald-600 dark:text-emerald-400 mx-auto mb-2" />
                <div className="text-3xl font-bold text-theme tabular-nums">
                  {scale.flowRate.toFixed(1)}
                </div>
                <div className="text-sm text-theme-muted">g/s flow</div>
              </div>
            </div>

            {/* Actions */}
            <div className="flex gap-3">
              <Button variant="secondary" onClick={tareScale}>
                Tare
              </Button>
              <Button variant="ghost" onClick={resetScale}>
                Reset
              </Button>
            </div>
          </>
        ) : (
          <div className="text-center py-8">
            <div className="w-16 h-16 bg-theme-secondary rounded-full flex items-center justify-center mx-auto mb-4">
              <Scale className="w-8 h-8 text-theme-muted" />
            </div>
            <h3 className="text-lg font-semibold text-theme mb-1">
              No Scale Connected
            </h3>
            <p className="text-sm text-theme-muted mb-4">
              Connect a Bluetooth scale to enable brew-by-weight
            </p>
            <Button
              variant="secondary"
              onClick={() => (window.location.href = "/settings#scale")}
            >
              Connect Scale
            </Button>
          </div>
        )}
      </Card>

      {/* Brew-by-Weight Settings - Requires scale */}
      <Card>
        <CardHeader
          action={
            <Toggle 
              checked={formState.enabled}
              onChange={(enabled) => setFormState({ ...formState, enabled })}
              label="Enable"
              disabled={!scale.connected}
            />
          }
        >
          <CardTitle icon={<Coffee className="w-5 h-5" />}>
            Brew-by-Weight
          </CardTitle>
        </CardHeader>

        {/* Show warning if no scale connected */}
        {!scale.connected && (
          <div className="mb-4 p-3 rounded-lg bg-amber-500/10 border border-amber-500/30 text-amber-700 dark:text-amber-300 text-sm flex items-center gap-2">
            <Scale className="w-4 h-4 flex-shrink-0" />
            <span>Connect a Bluetooth scale to enable brew-by-weight</span>
          </div>
        )}

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
          <Input
            label="Dose Weight"
            type="number"
            min={10}
            max={30}
            step={0.1}
            value={formState.doseWeight}
            onChange={(e) =>
              setFormState({
                ...formState,
                doseWeight: parseFloat(e.target.value) || 0,
              })
            }
            unit="g"
            hint="Coffee grounds"
          />

          <Input
            label="Target Weight"
            type="number"
            min={20}
            max={80}
            step={0.1}
            value={formState.targetWeight}
            onChange={(e) =>
              setFormState({
                ...formState,
                targetWeight: parseFloat(e.target.value) || 0,
              })
            }
            unit="g"
            hint="Desired output"
          />

          <div className="space-y-1.5">
            <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted">
              Ratio
            </label>
            <div className="h-[50px] flex items-center justify-center text-2xl font-bold text-theme bg-theme-secondary border border-theme rounded-xl">
              1:{ratio}
            </div>
          </div>

          <Input
            label="Stop Offset"
            type="number"
            min={0}
            max={10}
            step={0.5}
            value={formState.stopOffset}
            onChange={(e) =>
              setFormState({
                ...formState,
                stopOffset: parseFloat(e.target.value) || 0,
              })
            }
            unit="g"
            hint="For drip"
          />
        </div>

        <div className="flex items-center justify-between">
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={formState.autoTare}
              onChange={(e) =>
                setFormState({ ...formState, autoTare: e.target.checked })
              }
              className="w-4 h-4 rounded border-theme text-accent focus:ring-accent"
            />
            <span className="text-sm text-theme-secondary">
              Auto-tare when portafilter placed
            </span>
          </label>

          <Button onClick={saveSettings} loading={saving}>
            Save Settings
          </Button>
        </div>
      </Card>

      {/* Pre-Infusion Settings - Doesn't require scale */}
      <Card>
        <CardHeader
          action={
            <Toggle
              checked={preinfusionForm.enabled}
              onChange={(enabled) =>
                setPreinfusionForm({ ...preinfusionForm, enabled })
              }
              label="Enable"
            />
          }
        >
          <CardTitle icon={<Waves className="w-5 h-5" />}>
            Pre-Infusion
          </CardTitle>
        </CardHeader>

        <p className="text-sm text-theme-muted mb-4">
          Pre-infusion wets the coffee puck before full pressure extraction,
          improving flavor and reducing channeling.
        </p>

        {preinfusionForm.enabled && (
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
            <Input
              label="Pump ON Time"
              type="number"
              min={500}
              max={10000}
              step={100}
              value={preinfusionForm.onTimeMs}
              onChange={(e) =>
                setPreinfusionForm({
                  ...preinfusionForm,
                  onTimeMs: parseInt(e.target.value) || 0,
                })
              }
              unit="ms"
              hint="Duration pump runs to wet the puck (1000-5000ms typical)"
            />

            <Input
              label="Soak/Pause Time"
              type="number"
              min={0}
              max={30000}
              step={500}
              value={preinfusionForm.pauseTimeMs}
              onChange={(e) =>
                setPreinfusionForm({
                  ...preinfusionForm,
                  pauseTimeMs: parseInt(e.target.value) || 0,
                })
              }
              unit="ms"
              hint="Duration to let water absorb before full extraction (2000-10000ms typical)"
            />
          </div>
        )}

        {preinfusionForm.enabled && (
          <div className="p-4 rounded-xl bg-theme-secondary mb-4">
            <div className="text-sm font-medium text-theme mb-2">
              Brew Cycle Preview
            </div>
            <div className="flex items-center gap-2 text-xs text-theme-muted">
              <span className="px-2 py-1 rounded bg-blue-500/20 text-blue-600 dark:text-blue-400">
                Pump ON {(preinfusionForm.onTimeMs / 1000).toFixed(1)}s
              </span>
              <span>→</span>
              <span className="px-2 py-1 rounded bg-amber-500/20 text-amber-600 dark:text-amber-400">
                Soak {(preinfusionForm.pauseTimeMs / 1000).toFixed(1)}s
              </span>
              <span>→</span>
              <span className="px-2 py-1 rounded bg-green-500/20 text-green-600 dark:text-green-400">
                Full Extraction
              </span>
            </div>
            <div className="text-xs text-theme-muted mt-2">
              Total pre-infusion:{" "}
              {(
                (preinfusionForm.onTimeMs + preinfusionForm.pauseTimeMs) /
                1000
              ).toFixed(1)}
              s before full pressure
            </div>
          </div>
        )}

        <div className="flex justify-end">
          <Button onClick={savePreinfusion} loading={savingPreinfusion}>
            Save Pre-Infusion
          </Button>
        </div>
      </Card>
    </div>
  );
}
