import {
  Coffee,
  Thermometer,
  Zap,
  Wifi,
  Scale as ScaleIcon,
  Cloud,
  Clock,
  Palette,
  Server,
  Info,
} from 'lucide-react';

export type SettingsTab = 
  | 'machine' 
  | 'temperature' 
  | 'power' 
  | 'network' 
  | 'scale' 
  | 'cloud' 
  | 'time' 
  | 'appearance' 
  | 'system' 
  | 'about';

export interface TabConfig {
  id: SettingsTab;
  label: string;
  icon: React.ComponentType<{ className?: string }>;
}

export const getSettingsTabs = (isCloud: boolean): TabConfig[] => {
  const tabs: TabConfig[] = [
    { id: 'machine', label: 'Machine', icon: Coffee },
    { id: 'temperature', label: 'Temperature', icon: Thermometer },
    { id: 'power', label: 'Power', icon: Zap },
    { id: 'network', label: 'Network', icon: Wifi },
    { id: 'scale', label: 'Scale', icon: ScaleIcon },
  ];
  
  // Only show Cloud tab in local mode
  if (!isCloud) {
    tabs.push({ id: 'cloud', label: 'Cloud', icon: Cloud });
  }
  
  tabs.push(
    { id: 'time', label: 'Time', icon: Clock },
    { id: 'appearance', label: 'Theme', icon: Palette },
    { id: 'system', label: 'System', icon: Server },
    { id: 'about', label: 'About', icon: Info },
  );
  
  return tabs;
};

