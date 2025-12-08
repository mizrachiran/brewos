import { useState, useRef, useEffect } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { ConfirmDialog } from "@/components/ConfirmDialog";
import { Zap, AlertTriangle, Settings, ChevronRight } from "lucide-react";

export function PowerSettings() {
  const power = useStore((s) => s.power);
  const { sendCommand } = useCommand();

  // Track original values to detect changes
  const originalVoltageRef = useRef(power.voltage);
  const originalMaxCurrentRef = useRef(power.maxCurrent);

  const [voltage, setVoltage] = useState(power.voltage);
  const [maxCurrent, setMaxCurrent] = useState(power.maxCurrent);
  const [savingPower, setSavingPower] = useState(false);
  const [showPowerWarning, setShowPowerWarning] = useState(false);
  const [editingPower, setEditingPower] = useState(false);

  // Update local state when store values change (e.g., on device_info received)
  useEffect(() => {
    if (!editingPower) {
      setVoltage(power.voltage);
      setMaxCurrent(power.maxCurrent);
      originalVoltageRef.current = power.voltage;
      originalMaxCurrentRef.current = power.maxCurrent;
    }
  }, [power.voltage, power.maxCurrent, editingPower]);

  // Check if power settings have changed
  const hasPowerSettingsChanged =
    voltage !== originalVoltageRef.current ||
    maxCurrent !== originalMaxCurrentRef.current;

  // Handle save button click - show warning if settings changed
  const handleSavePower = () => {
    if (hasPowerSettingsChanged) {
      setShowPowerWarning(true);
    } else {
      savePower();
    }
  };

  // Actually save the power settings
  const savePower = () => {
    if (savingPower) return; // Prevent double-click
    setSavingPower(true);
    sendCommand(
      "set_power",
      { voltage, maxCurrent },
      { successMessage: "Power settings saved" }
    );
    // Update references to new values after save
    originalVoltageRef.current = voltage;
    originalMaxCurrentRef.current = maxCurrent;
    // Brief visual feedback for fire-and-forget WebSocket command
    setTimeout(() => {
      setSavingPower(false);
      setShowPowerWarning(false);
      setEditingPower(false);
    }, 600);
  };

  // Voltage label helper
  const getVoltageLabel = (v: number) => {
    if (v === 110) return "110V (US)";
    if (v === 220) return "220V (EU/AU)";
    if (v === 240) return "240V (UK)";
    return `${v}V`;
  };

  return (
    <>
      {/* Power Settings */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Zap className="w-5 h-5" />}>
            Electrical Configuration
          </CardTitle>
        </CardHeader>

        {!editingPower ? (
          /* View mode */
          <div className="space-y-0">
            <div className="flex items-center justify-between py-2 border-b border-theme">
              <span className="text-sm text-theme-muted">Mains Voltage</span>
              <span className="text-sm font-medium text-theme">
                {getVoltageLabel(voltage)}
              </span>
            </div>
            <div className="flex items-center justify-between py-2 border-b border-theme">
              <span className="text-sm text-theme-muted">Max Current</span>
              <span className="text-sm font-medium text-theme">
                {maxCurrent}A
              </span>
            </div>

            <button
              onClick={() => setEditingPower(true)}
              className="w-full flex items-center justify-between py-2.5 border-t border-theme text-left group transition-colors hover:opacity-80 mt-2"
            >
              <div className="flex items-center gap-3">
                <Settings className="w-4 h-4 text-theme-muted" />
                <span className="text-sm font-medium text-theme">
                  Configure Power
                </span>
              </div>
              <ChevronRight className="w-4 h-4 text-theme-muted group-hover:text-theme transition-colors" />
            </button>
          </div>
        ) : (
          /* Edit mode */
          <div className="space-y-4">
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
              <div className="space-y-1.5">
                <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted">
                  Mains Voltage
                </label>
                <select
                  value={voltage}
                  onChange={(e) => setVoltage(parseInt(e.target.value))}
                  className="input"
                >
                  <option value="110">110V (US/Japan)</option>
                  <option value="220">220V (EU/AU/Most of the World)</option>
                  <option value="240">240V (UK)</option>
                </select>
              </div>
              <Input
                label="Max Current"
                type="number"
                min={10}
                max={20}
                step={1}
                value={maxCurrent}
                onChange={(e) => setMaxCurrent(parseInt(e.target.value) || 10)}
                unit="A"
                hint="10A (AU), 13A (UK/Ireland), 15-20A (US/Canada), 16A (EU)"
              />
            </div>

            <div className="flex justify-end gap-3">
              <Button variant="ghost" onClick={() => setEditingPower(false)}>
                Cancel
              </Button>
              <Button
                onClick={() => {
                  handleSavePower();
                  if (!hasPowerSettingsChanged) {
                    setEditingPower(false);
                  }
                }}
                loading={savingPower}
                disabled={savingPower}
              >
                Save
              </Button>
            </div>
          </div>
        )}
      </Card>

      {/* Power Settings Warning Dialog */}
      <ConfirmDialog
        isOpen={showPowerWarning}
        onClose={() => setShowPowerWarning(false)}
        onConfirm={savePower}
        title="Changing Power Settings"
        description="You are about to modify critical electrical parameters."
        variant="warning"
        confirmText="I Understand, Save Settings"
        cancelText="Cancel"
        confirmLoading={savingPower}
      >
        <div className="space-y-4 text-sm">
          {/* Voltage change warning */}
          {voltage !== originalVoltageRef.current && (
            <div className="flex gap-3 p-3 rounded-lg bg-amber-500/10 border border-amber-500/20">
              <AlertTriangle className="w-5 h-5 text-amber-500 flex-shrink-0 mt-0.5" />
              <div>
                <p className="font-semibold text-amber-500">
                  Voltage Change Detected
                </p>
                <p className="text-theme-muted mt-1">
                  Changing from <strong>{originalVoltageRef.current}V</strong>{" "}
                  to <strong>{voltage}V</strong>.
                </p>
                <p className="text-theme-muted mt-2">
                  {voltage < originalVoltageRef.current ? (
                    <>
                      Lowering voltage settings when your mains supply is higher
                      can cause{" "}
                      <span className="text-amber-500 font-medium">
                        overheating, equipment damage, or fire hazards
                      </span>
                      . Only change this if you've physically moved the machine
                      to a location with different mains voltage.
                    </>
                  ) : (
                    <>
                      Setting voltage higher than your actual mains supply will
                      result in{" "}
                      <span className="text-amber-500 font-medium">
                        insufficient heating power and poor performance
                      </span>
                      . Only change this if you've physically moved the machine
                      to a location with different mains voltage.
                    </>
                  )}
                </p>
              </div>
            </div>
          )}

          {/* Max current change warning */}
          {maxCurrent !== originalMaxCurrentRef.current && (
            <div className="flex gap-3 p-3 rounded-lg bg-amber-500/10 border border-amber-500/20">
              <AlertTriangle className="w-5 h-5 text-amber-500 flex-shrink-0 mt-0.5" />
              <div>
                <p className="font-semibold text-amber-500">
                  Current Limit Change
                </p>
                <p className="text-theme-muted mt-1">
                  Changing from{" "}
                  <strong>{originalMaxCurrentRef.current}A</strong> to{" "}
                  <strong>{maxCurrent}A</strong>.
                </p>
                <p className="text-theme-muted mt-2">
                  {maxCurrent > originalMaxCurrentRef.current ? (
                    <>
                      Increasing the current limit beyond your circuit's
                      capacity can{" "}
                      <span className="text-amber-500 font-medium">
                        trip breakers, damage wiring, or create fire hazards
                      </span>
                      . Ensure your outlet and circuit can safely handle{" "}
                      <strong>{maxCurrent}A</strong> continuous load.
                    </>
                  ) : (
                    <>
                      Lowering the current limit may cause{" "}
                      <span className="text-amber-500 font-medium">
                        breakers to trip during high-power operations
                      </span>
                      , especially when using{" "}
                      <strong>Parallel heating strategy</strong> on dual boiler
                      machines. The system will try to draw more power than
                      allowed, triggering circuit protection.
                    </>
                  )}
                </p>
              </div>
            </div>
          )}

          {/* General advice */}
          <p className="text-theme-muted text-xs border-t border-theme pt-3">
            <strong>⚠️ Risk:</strong> Incorrect power settings can{" "}
            <span className="text-amber-500">trip your circuit breaker</span>,
            cause equipment damage, or create safety hazards. Ensure these
            values match your electrical installation. If unsure, consult a
            qualified electrician.
          </p>
        </div>
      </ConfirmDialog>
    </>
  );
}
