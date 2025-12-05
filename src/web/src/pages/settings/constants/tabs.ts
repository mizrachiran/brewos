import {
  Coffee,
  Wifi,
  Scale as ScaleIcon,
  Cloud,
  Globe,
  Palette,
  Server,
  Info,
} from "lucide-react";

export type SettingsTab =
  | "machine"
  | "network"
  | "scale"
  | "cloud"
  | "regional"
  | "appearance"
  | "system"
  | "about";

export interface TabConfig {
  id: SettingsTab;
  label: string;
  icon: React.ComponentType<{ className?: string }>;
}

export const getSettingsTabs = (): TabConfig[] => {
  return [
    { id: "machine", label: "Machine", icon: Coffee },
    { id: "scale", label: "Scale", icon: ScaleIcon },
    { id: "network", label: "Network", icon: Wifi },
    { id: "cloud", label: "Cloud", icon: Cloud },
    { id: "appearance", label: "Theme", icon: Palette },
    { id: "regional", label: "Regional", icon: Globe },
    { id: "system", label: "System", icon: Server },
    { id: "about", label: "About", icon: Info },
  ];
};
