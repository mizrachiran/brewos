import { useLocation, useNavigate } from 'react-router-dom';
import { useAppStore } from '@/lib/mode';
import { getSettingsTabs, SettingsTab } from './constants/tabs';
import {
  SettingsNav,
  MachineSettings,
  TemperatureSettings,
  PowerSettings,
  NetworkSettings,
  ScaleSettings,
  CloudSettings,
  TimeSettings,
  RegionalSettings,
  ThemeSettings,
  SystemSettings,
  PushNotificationSettings,
  AboutSection,
} from './components';

export function Settings() {
  const location = useLocation();
  const navigate = useNavigate();
  const { mode } = useAppStore();
  
  const isCloud = mode === 'cloud';
  const SETTINGS_TABS = getSettingsTabs(isCloud);

  // Get active tab from URL hash, default to 'machine'
  const hashTab = location.hash.slice(1) as SettingsTab;
  const activeTab = SETTINGS_TABS.some(t => t.id === hashTab) ? hashTab : 'machine';
  
  const setActiveTab = (tab: SettingsTab) => {
    navigate({ hash: tab }, { replace: true });
  };

  return (
    <div className="space-y-6">
      {/* Tab Navigation */}
      <SettingsNav
        tabs={SETTINGS_TABS}
        activeTab={activeTab}
        onTabChange={setActiveTab}
      />

      {/* Device Identity */}
      {activeTab === 'machine' && <MachineSettings />}

      {/* Temperature Settings */}
      {activeTab === 'temperature' && <TemperatureSettings />}

      {/* Power & Energy Settings */}
      {activeTab === 'power' && <PowerSettings />}

      {/* Network Settings (WiFi + MQTT) */}
      {activeTab === 'network' && <NetworkSettings />}

      {/* Scale Settings */}
      {activeTab === 'scale' && <ScaleSettings />}

      {/* Cloud Settings (local mode only) */}
      {activeTab === 'cloud' && !isCloud && <CloudSettings />}

      {/* Time Settings */}
      {activeTab === 'time' && (
        <>
          <TimeSettings />
          <RegionalSettings />
        </>
      )}

      {/* Appearance / Theme Settings */}
      {activeTab === 'appearance' && <ThemeSettings />}

      {/* System Settings */}
      {activeTab === 'system' && (
        <>
          <SystemSettings />
          {isCloud && <PushNotificationSettings />}
        </>
      )}

      {/* About */}
      {activeTab === 'about' && <AboutSection />}
    </div>
  );
}

