import { Card } from "@/components/Card";
import { Badge } from "@/components/Badge";
import { Power, Zap, Droplets } from "lucide-react";
import { cn, getMachineStateLabel, getMachineStateColor } from "@/lib/utils";

type MachineMode = "standby" | "on" | "eco";

interface MachineStatusCardProps {
  mode: string;
  state: string;
  isDualBoiler: boolean;
  onSetMode: (mode: string) => void;
  onPowerOn: () => void;
}

export function MachineStatusCard({
  mode,
  state,
  onSetMode,
  onPowerOn,
}: MachineStatusCardProps) {
  return (
    <Card>
      <div className="space-y-6">
        <StatusHeader mode={mode} state={state} />
        <PowerControls mode={mode} onSetMode={onSetMode} onPowerOn={onPowerOn} />
      </div>
    </Card>
  );
}

function StatusHeader({ mode, state }: { mode: string; state: string }) {
  return (
    <div className="flex items-center justify-between">
      <div className="flex items-center gap-4">
        <div
          className={cn(
            "w-16 h-16 rounded-2xl flex items-center justify-center transition-all",
            mode === "on"
              ? "bg-accent/20 text-accent"
              : mode === "eco"
              ? "bg-emerald-500/20 text-emerald-500"
              : "bg-theme-tertiary text-theme-muted"
          )}
        >
          <Power
            className={cn(
              "w-8 h-8 transition-transform",
              mode === "on" && "animate-pulse"
            )}
          />
        </div>
        <div>
          <h2 className="text-xl font-bold text-theme mb-1">Machine Status</h2>
          <Badge className={getMachineStateColor(state)}>
            {getMachineStateLabel(state)}
          </Badge>
        </div>
      </div>
    </div>
  );
}

interface PowerControlsProps {
  mode: string;
  onSetMode: (mode: string) => void;
  onPowerOn: () => void;
}

function PowerControls({ mode, onSetMode, onPowerOn }: PowerControlsProps) {
  const modes: { id: MachineMode; icon: typeof Power; label: string }[] = [
    { id: "standby", icon: Power, label: "Standby" },
    { id: "on", icon: Zap, label: "On" },
    { id: "eco", icon: Droplets, label: "Eco" },
  ];

  return (
    <div className="border-t border-theme pt-4">
      <div className="flex flex-col sm:flex-row gap-3">
        {modes.map((m) => {
          const Icon = m.icon;
          const isActive = mode === m.id;
          const handleClick =
            m.id === "on" ? onPowerOn : () => onSetMode(m.id);

          return (
            <button
              key={m.id}
              onClick={handleClick}
              className={cn(
                "flex-1 px-4 py-3 rounded-xl text-sm font-semibold transition-all",
                "flex items-center justify-center gap-2",
                isActive
                  ? "nav-active"
                  : "bg-theme-secondary text-theme-secondary hover:bg-theme-tertiary"
              )}
            >
              <Icon className="w-4 h-4" />
              <span>{m.label}</span>
            </button>
          );
        })}
      </div>
    </div>
  );
}

