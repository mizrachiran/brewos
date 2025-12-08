import { useState, useEffect } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Toggle } from "@/components/Toggle";
import { PageHeader } from "@/components/PageHeader";
import { ScaleStatusCard } from "@/components/ScaleStatusCard";
import { Coffee, Scale, Waves } from "lucide-react";

export function Brewing() {
  const bbw = useStore((s) => s.bbw);
  const scale = useStore((s) => s.scale);
  const preinfusion = useStore((s) => s.preinfusion);
  const { sendCommand } = useCommand();

  // Local form state for BBW
  const [formState, setFormState] = useState(bbw);
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
        title="Brew Settings"
        subtitle="Configure your brew parameters"
      />

      {/* Scale Status - First, as it's the foundation for BBW */}
      <ScaleStatusCard
        actions={
          <div className="flex gap-3">
            <Button variant="secondary" onClick={tareScale}>
              Tare
            </Button>
            <Button variant="ghost" onClick={resetScale}>
              Reset
            </Button>
          </div>
        }
      />

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
