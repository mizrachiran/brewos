import { useState, useEffect } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Activity, Settings, ChevronRight, Loader2, CheckCircle2, XCircle } from "lucide-react";
import { PowerMeterStatus } from "./PowerMeterStatus";

type PowerSource = "none" | "hardware" | "mqtt";

interface PowerMeterConfig extends Record<string, unknown> {
  source: PowerSource;
  meterType?: string;
  slaveAddr?: number;
  baudRate?: number;
  topic?: string;
  format?: string;
}

export function PowerMeterSettings() {
  const powerMeter = useStore((s) => s.power.meter);
  const { sendCommand } = useCommand();

  const [editing, setEditing] = useState(false);
  const [saving, setSaving] = useState(false);
  
  // Configuration state
  const [source, setSource] = useState<PowerSource>("none");
  const [meterType, setMeterType] = useState<string>("auto");
  const [mqttTopic, setMqttTopic] = useState<string>("");
  const [mqttFormat, setMqttFormat] = useState<string>("auto");

  // Initialize from store
  useEffect(() => {
    if (powerMeter) {
      setSource(powerMeter.source);
      setMeterType(powerMeter.meterType || "auto");
      // MQTT topic would come from config (not in status)
    }
  }, [powerMeter]);

  const handleSave = async () => {
    setSaving(true);

    const config: PowerMeterConfig = {
      source: source,
    };

    if (source === "hardware") {
      config.meterType = meterType;
      if (meterType !== "auto") {
        // Include specific config if not auto-detecting
        config.slaveAddr = 0;  // Use default
        config.baudRate = 0;   // Use default
      }
    } else if (source === "mqtt") {
      config.topic = mqttTopic;
      config.format = mqttFormat;
    }

    sendCommand("configure_power_meter", config, {
      successMessage: "Power meter configuration saved",
    });

    // Brief visual feedback
    setTimeout(() => {
      setSaving(false);
      setEditing(false);
    }, 600);
  };

  const handleAutoDetect = () => {
    sendCommand("start_power_meter_discovery", {}, {
      successMessage: "Starting auto-detection...",
    });
  };

  const handleCancel = () => {
    // Reset to current values
    if (powerMeter) {
      setSource(powerMeter.source);
      setMeterType(powerMeter.meterType || "auto");
    } else {
      setSource("none");
      setMeterType("auto");
    }
    setEditing(false);
  };

  const hasChanges =
    (powerMeter?.source !== source) ||
    (source === "hardware" && powerMeter?.meterType !== meterType) ||
    (source === "mqtt" && mqttTopic.length > 0);

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Activity className="w-5 h-5" />}>
          Power Metering
        </CardTitle>
      </CardHeader>

      {!editing ? (
        /* View mode */
        <div className="space-y-0">
          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Source</span>
            <span className="text-sm font-medium text-theme capitalize">
              {powerMeter?.source === "hardware" && "Hardware Module"}
              {powerMeter?.source === "mqtt" && "MQTT Topic"}
              {(!powerMeter || powerMeter?.source === "none") && "None"}
            </span>
          </div>

          {powerMeter?.source === "hardware" && (
            <div className="flex items-center justify-between py-2 border-b border-theme">
              <span className="text-sm text-theme-muted">Meter Type</span>
              <span className="text-sm font-medium text-theme">
                {powerMeter?.meterType || "Unknown"}
              </span>
            </div>
          )}

          {powerMeter?.source === "mqtt" && (
            <div className="flex items-center justify-between py-2 border-b border-theme">
              <span className="text-sm text-theme-muted">MQTT Topic</span>
              <span className="text-sm font-medium text-theme truncate max-w-[200px]">
                {mqttTopic || "Not configured"}
              </span>
            </div>
          )}

          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Status</span>
            <div className="flex items-center gap-2">
              {powerMeter?.connected ? (
                <>
                  <CheckCircle2 className="w-4 h-4 text-green-500" />
                  <span className="text-sm font-medium text-green-500">
                    Connected
                  </span>
                </>
              ) : (
                <>
                  <XCircle className="w-4 h-4 text-theme-muted" />
                  <span className="text-sm font-medium text-theme-muted">
                    {powerMeter?.source === "none" ? "Disabled" : "Disconnected"}
                  </span>
                </>
              )}
            </div>
          </div>

          {/* Show current readings if connected */}
          {powerMeter?.connected && powerMeter?.reading && (
            <PowerMeterStatus />
          )}

          <button
            onClick={() => setEditing(true)}
            className="w-full flex items-center justify-between py-2.5 border-t border-theme text-left group transition-colors hover:opacity-80 mt-2"
          >
            <div className="flex items-center gap-3">
              <Settings className="w-4 h-4 text-theme-muted" />
              <span className="text-sm font-medium text-theme">
                Configure Power Metering
              </span>
            </div>
            <ChevronRight className="w-4 h-4 text-theme-muted group-hover:text-theme transition-colors" />
          </button>
        </div>
      ) : (
        /* Edit mode */
        <div className="space-y-4">
          <p className="text-sm text-theme-muted">
            Configure how BrewOS monitors machine power consumption.
          </p>

          {/* Source Selection */}
          <div className="space-y-1.5">
            <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted">
              Power Meter Source
            </label>
            <select
              value={source}
              onChange={(e) => setSource(e.target.value as PowerSource)}
              className="input"
            >
              <option value="none">None (Disabled)</option>
              <option value="hardware">Hardware Module (UART/RS485)</option>
              <option value="mqtt">MQTT Topic (Smart Plug)</option>
            </select>
          </div>

          {/* Hardware Module Configuration */}
          {source === "hardware" && (
            <div className="space-y-3">
              <div className="space-y-1.5">
                <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted">
                  Meter Type
                </label>
                <select
                  value={meterType}
                  onChange={(e) => setMeterType(e.target.value)}
                  className="input"
                >
                  <option value="auto">Auto-detect</option>
                  <option value="PZEM-004T V3">PZEM-004T V3</option>
                  <option value="JSY-MK-163T">JSY-MK-163T</option>
                  <option value="JSY-MK-194T">JSY-MK-194T</option>
                  <option value="Eastron SDM120">Eastron SDM120</option>
                  <option value="Eastron SDM230">Eastron SDM230</option>
                </select>
              </div>

              {meterType === "auto" && (
                <Button
                  onClick={handleAutoDetect}
                  variant="secondary"
                  className="w-full"
                  disabled={powerMeter?.discovering}
                  loading={powerMeter?.discovering}
                >
                  {powerMeter?.discovering ? (
                    <>
                      <Loader2 className="w-4 h-4 mr-2 animate-spin" />
                      {powerMeter?.discoveryProgress || "Detecting..."}
                    </>
                  ) : (
                    "Auto-Detect Meter"
                  )}
                </Button>
              )}

              {powerMeter?.meterType && meterType === "auto" && !powerMeter?.discovering && (
                <div className="flex items-center gap-2 p-3 rounded-lg bg-green-500/10 border border-green-500/20">
                  <CheckCircle2 className="w-4 h-4 text-green-500 flex-shrink-0" />
                  <div className="text-sm">
                    <p className="font-medium text-green-500">Detected</p>
                    <p className="text-theme-muted">{powerMeter.meterType}</p>
                  </div>
                </div>
              )}

              <p className="text-xs text-theme-muted">
                Connect meter to J17 connector (6-pin JST-XH). See hardware documentation for wiring.
              </p>
            </div>
          )}

          {/* MQTT Configuration */}
          {source === "mqtt" && (
            <div className="space-y-3">
              <Input
                label="MQTT Topic"
                type="text"
                value={mqttTopic}
                onChange={(e) => setMqttTopic(e.target.value)}
                placeholder="shellies/shelly-plug-XXX/status"
                hint="Full MQTT topic path that publishes power data"
              />

              <div className="space-y-1.5">
                <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted">
                  Data Format
                </label>
                <select
                  value={mqttFormat}
                  onChange={(e) => setMqttFormat(e.target.value)}
                  className="input"
                >
                  <option value="auto">Auto-detect</option>
                  <option value="shelly">Shelly Plug</option>
                  <option value="tasmota">Tasmota (SENSOR)</option>
                  <option value="generic">Generic JSON</option>
                </select>
              </div>

              <p className="text-xs text-theme-muted">
                Supports Shelly Plug, Tasmota-flashed smart plugs, and generic JSON formats.
                MQTT must be enabled in Network settings.
              </p>
            </div>
          )}

          {source === "none" && (
            <div className="p-3 rounded-lg bg-theme-muted/10">
              <p className="text-sm text-theme-muted">
                Power metering disabled. Energy consumption will not be tracked.
              </p>
            </div>
          )}

          {/* Action Buttons */}
          <div className="flex justify-end gap-3 pt-2">
            <Button variant="ghost" onClick={handleCancel}>
              Cancel
            </Button>
            <Button
              onClick={handleSave}
              loading={saving}
              disabled={saving || !hasChanges}
            >
              Save Configuration
            </Button>
          </div>
        </div>
      )}
    </Card>
  );
}

