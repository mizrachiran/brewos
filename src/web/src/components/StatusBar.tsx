import { memo } from "react";
import { Power, Wifi, Bluetooth, Cloud } from "lucide-react";
import { cn } from "@/lib/utils";
import { useStore } from "@/lib/store";

interface StatusIndicatorProps {
  icon: typeof Power;
  active: boolean;
  title: string;
  activeColor?: string;
}

const StatusIndicator = memo(function StatusIndicator({
  icon: Icon,
  active,
  title,
  activeColor = "text-emerald-500",
}: StatusIndicatorProps) {
  return (
    <div
      className={cn(
        "transition-colors duration-300",
        active ? activeColor : "text-theme-muted/30"
      )}
      title={title}
    >
      <Icon className="w-4 h-4" strokeWidth={2} />
    </div>
  );
});

export const StatusBar = memo(function StatusBar() {
  const machineMode = useStore((s) => s.machine.mode);
  const wifiConnected = useStore((s) => s.wifi.connected);
  const scaleConnected = useStore((s) => s.scale.connected);
  const cloudConnected = useStore((s) => s.cloud.connected);

  const isPoweredOn = machineMode !== "standby";

  return (
    <div className="inline-flex items-center gap-3 px-3 py-1.5 rounded-full bg-theme-secondary/50">
      <StatusIndicator
        icon={Power}
        active={isPoweredOn}
        title={isPoweredOn ? "Machine On" : "Machine Off"}
        activeColor="text-emerald-500"
      />
      <StatusIndicator
        icon={Wifi}
        active={wifiConnected}
        title={wifiConnected ? "WiFi Connected" : "WiFi Disconnected"}
        activeColor="text-sky-500"
      />
      <StatusIndicator
        icon={Bluetooth}
        active={scaleConnected}
        title={scaleConnected ? "Scale Connected" : "Scale Disconnected"}
        activeColor="text-blue-500"
      />
      <StatusIndicator
        icon={Cloud}
        active={cloudConnected}
        title={cloudConnected ? "Cloud Connected" : "Cloud Disconnected"}
        activeColor="text-violet-500"
      />
    </div>
  );
});

