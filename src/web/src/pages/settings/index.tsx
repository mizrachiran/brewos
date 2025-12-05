import { useLocation, useNavigate } from "react-router-dom";
import { useAppStore } from "@/lib/mode";
import { PageHeader } from "@/components/PageHeader";
import { getSettingsTabs, SettingsTab } from "./constants/tabs";
import { isDemoMode } from "@/lib/demo-mode";
import {
  SettingsNav,
  MachineSettings,
  TemperatureSettings,
  PowerSettings,
  NetworkSettings,
  ScaleSettings,
  CloudSettings,
  CloudShareSettings,
  TimeSettings,
  RegionalSettings,
  UnitsSettings,
  ThemeSettings,
  SystemSettings,
  PushNotificationSettings,
  AboutSection,
} from "./components";

export function Settings() {
  const location = useLocation();
  const navigate = useNavigate();
  const { mode } = useAppStore();

  const isCloud = mode === "cloud";
  const isDemo = isDemoMode();
  const SETTINGS_TABS = getSettingsTabs();

  // Get active tab from URL hash, default to 'machine'
  const hashTab = location.hash.slice(1) as SettingsTab;
  const activeTab = SETTINGS_TABS.some((t) => t.id === hashTab)
    ? hashTab
    : "machine";

  const setActiveTab = (tab: SettingsTab) => {
    navigate({ hash: tab }, { replace: true });
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <PageHeader
        title="Settings"
        subtitle="Configure your machine preferences"
      />

      {/* Tab Navigation */}
      <SettingsNav
        tabs={SETTINGS_TABS}
        activeTab={activeTab}
        onTabChange={setActiveTab}
      />

      {/* Machine Settings (Identity + Temperature + Power) */}
      {activeTab === "machine" && (
        <>
          <MachineSettings />
          <TemperatureSettings />
          <PowerSettings />
        </>
      )}

      {/* Network Settings (WiFi + MQTT) */}
      {activeTab === "network" && <NetworkSettings />}

      {/* Scale Settings */}
      {activeTab === "scale" && <ScaleSettings />}

      {/* Cloud Settings */}
      {activeTab === "cloud" && (
        <>
          {/* Local mode: Show ESP32 pairing and cloud configuration */}
          {/* Cloud mode: Show device sharing and cloud status */}
          {isCloud && !isDemo ? <CloudShareSettings /> : <CloudSettings />}
          {/* Push notification settings are shown in all modes */}
          <PushNotificationSettings />
        </>
      )}

      {/* Regional Settings (Time, Locale, Units) */}
      {activeTab === "regional" && (
        <>
          <TimeSettings />
          <RegionalSettings />
          <UnitsSettings />
        </>
      )}

      {/* Appearance / Theme Settings */}
      {activeTab === "appearance" && <ThemeSettings />}

      {/* System Settings */}
      {activeTab === "system" && <SystemSettings />}

      {/* About */}
      {activeTab === "about" && <AboutSection />}
    </div>
  );
}
