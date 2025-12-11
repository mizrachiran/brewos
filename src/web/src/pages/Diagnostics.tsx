import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { useMobileLandscape } from "@/lib/useMobileLandscape";
import { PageHeader } from "@/components/PageHeader";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Button } from "@/components/Button";
import { Badge } from "@/components/Badge";
import { useNavigate } from "react-router-dom";
import {
  Activity,
  Thermometer,
  Gauge,
  Droplets,
  Zap,
  Wifi,
  Speaker,
  Lightbulb,
  Play,
  CheckCircle2,
  XCircle,
  AlertTriangle,
  MinusCircle,
  Loader2,
  RefreshCw,
  Cable,
  ArrowLeft,
  HelpCircle,
} from "lucide-react";
import type {
  DiagnosticResult,
  DiagnosticStatus,
  MachineType,
} from "@/lib/types";
import { DIAGNOSTIC_TESTS, getTestsForMachineType } from "@/lib/types";

// Power meter test ID for dynamic name display
const POWER_METER_TEST_ID = 0x0a;

// Map test IDs to icons
function getTestIcon(testId: number) {
  const icons: Record<number, React.ReactNode> = {
    // Temperature Sensors (T1, T2)
    0x01: <Thermometer className="w-5 h-5" />, // Brew Boiler NTC
    0x02: <Thermometer className="w-5 h-5" />, // Steam Boiler NTC

    // Pressure Sensor (P1)
    0x04: <Gauge className="w-5 h-5" />, // Pressure Transducer

    // Water Level Sensors (S1, S2, S3)
    0x05: <Droplets className="w-5 h-5" />, // Water Reservoir & Tank Level
    0x0e: <Droplets className="w-5 h-5" />, // Steam Boiler Level Probe

    // Brew Control (S4)
    0x0f: <Activity className="w-5 h-5" />, // Brew Handle/Lever Switch

    // Heater SSRs (SSR1, SSR2)
    0x06: <Zap className="w-5 h-5" />, // Brew Heater SSR
    0x07: <Zap className="w-5 h-5" />, // Steam Heater SSR

    // Relays (K1, K2, K3)
    0x10: <Lightbulb className="w-5 h-5" />, // Water Status LED Relay (K1)
    0x08: <Droplets className="w-5 h-5" />, // Water Pump Relay (K2)
    0x09: <Activity className="w-5 h-5" />, // Brew Solenoid Relay (K3)

    // Communication
    0x0b: <Wifi className="w-5 h-5" />, // ESP32 Communication
    0x0a: <Cable className="w-5 h-5" />, // Power Meter (PZEM, JSY, Eastron)

    // User Interface
    0x0c: <Speaker className="w-5 h-5" />, // Buzzer
    0x0d: <Lightbulb className="w-5 h-5" />, // Status LED
  };
  return icons[testId] || <HelpCircle className="w-5 h-5" />;
}

function getStatusInfo(status: DiagnosticStatus) {
  switch (status) {
    case "pass":
      return {
        icon: <CheckCircle2 className="w-5 h-5" />,
        color: "text-emerald-500",
        bgColor: "bg-emerald-500/10",
        borderColor: "border-emerald-500/30",
        label: "Pass",
      };
    case "fail":
      return {
        icon: <XCircle className="w-5 h-5" />,
        color: "text-red-500",
        bgColor: "bg-red-500/10",
        borderColor: "border-red-500/30",
        label: "Fail",
      };
    case "warn":
      return {
        icon: <AlertTriangle className="w-5 h-5" />,
        color: "text-amber-500",
        bgColor: "bg-amber-500/10",
        borderColor: "border-amber-500/30",
        label: "Warning",
      };
    case "skip":
      return {
        icon: <MinusCircle className="w-5 h-5" />,
        color: "text-slate-400",
        bgColor: "bg-slate-500/10",
        borderColor: "border-slate-500/30",
        label: "Skipped",
      };
    case "running":
      return {
        icon: <Loader2 className="w-5 h-5 animate-spin" />,
        color: "text-blue-500",
        bgColor: "bg-blue-500/10",
        borderColor: "border-blue-500/30",
        label: "Running",
      };
  }
}

function TroubleshootItem({
  title,
  description,
}: {
  title: string;
  description: string;
}) {
  return (
    <div className="flex gap-3">
      <div className="w-1 rounded-full bg-theme-tertiary flex-shrink-0" />
      <div>
        <h4 className="font-medium text-theme">{title}</h4>
        <p className="text-theme-muted">{description}</p>
      </div>
    </div>
  );
}

function DiagnosticResultRow({
  result,
  configuredMeterType,
}: {
  result: DiagnosticResult;
  configuredMeterType?: string | null;
}) {
  const statusInfo = getStatusInfo(result.status);
  const testMeta = DIAGNOSTIC_TESTS.find((t) => t.id === result.testId);
  const isOptional = testMeta?.optional ?? false;

  // For power meter test, show the configured meter type if available
  const displayName =
    result.testId === POWER_METER_TEST_ID && configuredMeterType
      ? `Power Meter (${configuredMeterType})`
      : result.name;

  return (
    <div className="flex items-center gap-4 p-4 rounded-xl bg-theme-secondary border border-theme">
      <div className="flex-shrink-0 text-theme-muted">
        {getTestIcon(result.testId)}
      </div>

      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2">
          <h4 className="font-medium text-theme truncate">{displayName}</h4>
          {isOptional && (
            <span className="text-[10px] px-1.5 py-0.5 rounded bg-theme-tertiary text-theme-muted uppercase">
              Optional
            </span>
          )}
        </div>
        <p className="text-sm text-theme-muted truncate">{result.message}</p>
      </div>

      <div className={`flex items-center gap-1.5 ${statusInfo.color}`}>
        {statusInfo.icon}
        <span className="text-sm font-medium hidden sm:inline">
          {statusInfo.label}
        </span>
      </div>
    </div>
  );
}

export function Diagnostics() {
  const navigate = useNavigate();
  const diagnostics = useStore((s) => s.diagnostics);
  const setDiagnosticsRunning = useStore((s) => s.setDiagnosticsRunning);
  const resetDiagnostics = useStore((s) => s.resetDiagnostics);
  const pico = useStore((s) => s.pico);
  const device = useStore((s) => s.device);
  const powerMeter = useStore((s) => s.power.meter);
  const { sendCommand } = useCommand();

  // Get configured power meter type for dynamic display
  const configuredMeterType =
    powerMeter?.source === "hardware" && powerMeter?.meterType
      ? powerMeter.meterType
      : null;

  // Get tests applicable to this machine type
  const machineType = (device.machineType || "dual_boiler") as MachineType;
  const applicableTests = getTestsForMachineType(machineType);
  const requiredTests = applicableTests.filter((t) => !t.optional);
  const optionalTests = applicableTests.filter((t) => t.optional);

  const runDiagnostics = () => {
    setDiagnosticsRunning(true);
    sendCommand("run_diagnostics");
  };

  const { header, results, isRunning, timestamp } = diagnostics;
  const hasResults = results.length > 0 || header.testCount > 0;

  const overallStatus: DiagnosticStatus = isRunning
    ? "running"
    : header.failCount > 0
    ? "fail"
    : header.warnCount > 0
    ? "warn"
    : header.passCount > 0
    ? "pass"
    : "skip";

  const overallInfo = getStatusInfo(overallStatus);
  const isMobileLandscape = useMobileLandscape();
  const sectionGap = isMobileLandscape ? "space-y-3" : "space-y-6";

  return (
    <div className={sectionGap}>
      <PageHeader
        title="Hardware Diagnostics"
        subtitle="Test hardware wiring and component functionality"
        action={
          <Button
            variant="ghost"
            size="sm"
            onClick={() => navigate("/settings")}
          >
            <ArrowLeft className="w-4 h-4" />
            Settings
          </Button>
        }
      />

      {/* Info Card */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Activity className="w-5 h-5" />}>
            Run Self-Tests
          </CardTitle>
        </CardHeader>

        <p className="text-theme-muted mb-4">
          Run self-tests to verify hardware wiring and component functionality.
          Tests are tailored to your{" "}
          <span className="text-theme font-medium">
            {machineType.replace("_", " ")}
          </span>{" "}
          machine configuration.
        </p>

        <div className="flex flex-wrap gap-4 text-sm mb-6">
          <div>
            <span className="text-theme-muted">Required tests:</span>{" "}
            <span className="font-medium text-theme">
              {requiredTests.length}
            </span>
          </div>
          <div>
            <span className="text-theme-muted">Optional tests:</span>{" "}
            <span className="font-medium text-theme">
              {optionalTests.length}
            </span>
          </div>
        </div>

        {/* Warning */}
        <div className="p-4 bg-amber-500/10 border border-amber-500/20 rounded-xl mb-6">
          <div className="flex items-start gap-3">
            <AlertTriangle className="w-5 h-5 text-amber-500 flex-shrink-0 mt-0.5" />
            <div>
              <h4 className="font-medium text-amber-500 mb-1">
                Before Running
              </h4>
              <ul className="text-sm text-theme-muted space-y-1">
                <li>• Ensure water reservoir is filled</li>
                <li>• Machine should be cold (ambient temperature)</li>
                <li>• Relays/SSRs will activate briefly during tests</li>
                <li>• Buzzer will chirp and LED will flash</li>
              </ul>
            </div>
          </div>
        </div>

        {/* Run Button */}
        <div className="flex flex-col sm:flex-row items-center gap-4">
          <Button
            onClick={runDiagnostics}
            disabled={isRunning || !pico.connected}
            className="w-full sm:w-auto"
          >
            {isRunning ? (
              <>
                <Loader2 className="w-4 h-4 animate-spin" />
                Running Diagnostics...
              </>
            ) : (
              <>
                <Play className="w-4 h-4" />
                Run All Tests
              </>
            )}
          </Button>

          {hasResults && !isRunning && (
            <Button
              variant="ghost"
              onClick={resetDiagnostics}
              className="w-full sm:w-auto"
            >
              <RefreshCw className="w-4 h-4" />
              Clear Results
            </Button>
          )}

          {!pico.connected && (
            <p className="text-sm text-red-500">
              Machine controller not connected. Connect to run diagnostics.
            </p>
          )}
        </div>
      </Card>

      {/* Results Summary */}
      {hasResults && (
        <Card>
          <CardHeader>
            <CardTitle icon={overallInfo.icon}>
              <span className={overallInfo.color}>
                {isRunning
                  ? "Running Tests..."
                  : header.failCount > 0
                  ? "Issues Detected"
                  : header.warnCount > 0
                  ? "Completed with Warnings"
                  : "All Tests Passed"}
              </span>
            </CardTitle>
            {!isRunning && header.durationMs > 0 && (
              <Badge variant="default">
                {(header.durationMs / 1000).toFixed(1)}s
              </Badge>
            )}
          </CardHeader>

          {/* Stats Grid */}
          <div className="grid grid-cols-5 gap-2 mb-6">
            <div className="p-3 text-center">
              <p className="text-2xl font-bold text-theme">
                {header.testCount}
              </p>
              <p className="text-xs text-theme-muted">Total</p>
            </div>
            <div className="p-3 text-center">
              <p className="text-2xl font-bold text-emerald-500">
                {header.passCount}
              </p>
              <p className="text-xs text-theme-muted">Passed</p>
            </div>
            <div className="p-3 text-center">
              <p className="text-2xl font-bold text-red-500">
                {header.failCount}
              </p>
              <p className="text-xs text-theme-muted">Failed</p>
            </div>
            <div className="p-3 text-center">
              <p className="text-2xl font-bold text-amber-500">
                {header.warnCount}
              </p>
              <p className="text-xs text-theme-muted">Warnings</p>
            </div>
            <div className="p-3 text-center">
              <p className="text-2xl font-bold text-theme-muted">
                {header.skipCount}
              </p>
              <p className="text-xs text-theme-muted">Skipped</p>
            </div>
          </div>

          {/* Results List */}
          {results.length > 0 && (
            <div className="space-y-3">
              <h3 className="text-sm font-medium text-theme-muted uppercase tracking-wider">
                Test Results
              </h3>
              {results.map((result, index) => (
                <DiagnosticResultRow
                  key={`${result.testId}-${index}`}
                  result={result}
                  configuredMeterType={configuredMeterType}
                />
              ))}
            </div>
          )}

          {/* Timestamp */}
          {timestamp > 0 && (
            <p className="text-xs text-theme-muted mt-4 text-center">
              Last run: {new Date(timestamp).toLocaleString()}
            </p>
          )}
        </Card>
      )}

      {/* Help Section */}
      <Card>
        <CardHeader>
          <CardTitle icon={<HelpCircle className="w-5 h-5" />}>
            Troubleshooting
          </CardTitle>
        </CardHeader>

        <div className="space-y-4 text-sm">
          <TroubleshootItem
            title="Temperature Sensors (T1, T2 - NTC)"
            description="50kΩ NTC thermistors with optimized pull-ups (3.3kΩ brew, 1.2kΩ steam). Reading 0 or max = disconnected. Dual boiler tests both, single boiler only T1."
          />
          <TroubleshootItem
            title="Water Level Sensors (S1, S2, S3)"
            description="S1/S2: Reservoir and tank float switches on GPIO2/3. S3: Steam boiler probe via OPA342/TLV3201 AC sensing on GPIO4. Low level warning is normal if tank not full."
          />
          <TroubleshootItem
            title="Brew Handle Switch (S4)"
            description="Microswitch on GPIO5 detects lever pull. Test checks switch responds. If stuck, clean or replace microswitch."
          />
          <TroubleshootItem
            title="Heater SSRs (SSR1, SSR2)"
            description="5V trigger signals on GPIO13/14. Test activates briefly. If test passes but heater fails, check external SSR wiring and AC connections."
          />
          <TroubleshootItem
            title="Relays (K1, K2, K3)"
            description="K2 (pump) and K3 (solenoid) are required. K1 (water LED) is optional. Listen for relay click. No click = relay or driver issue."
          />
          <TroubleshootItem
            title="Pressure Transducer (P1 - Optional)"
            description="YD4060 0-16 bar on ADC2 (GPIO28). Fill reservoir before testing. Skip if not installed."
          />
          <TroubleshootItem
            title="Communication & Power Meter"
            description="Network controller connection via UART (921600 baud). Power meter is optional, supports PZEM-004T, JSY-MK-163T/194T, and Eastron SDM120/230 via Modbus."
          />
        </div>
      </Card>
    </div>
  );
}
