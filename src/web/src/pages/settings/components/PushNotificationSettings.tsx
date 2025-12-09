import { useState, useEffect, useCallback } from "react";
import { usePushNotifications } from "@/hooks/usePushNotifications";
import { useAppBadge } from "@/hooks/useAppBadge";
import { useStore } from "@/lib/store";
import { isDemoMode } from "@/lib/demo-mode";
import {
  Card,
  CardHeader,
  CardTitle,
  CardDescription,
} from "@/components/Card";
import { Button } from "@/components/Button";
import { Badge } from "@/components/Badge";
import { Toggle } from "@/components/Toggle";
import {
  Bell,
  BellOff,
  Check,
  X,
  AlertCircle,
  Coffee,
  Droplet,
  Wrench,
  AlertTriangle,
  Clock,
  Wifi,
  Settings2,
  Loader2,
  Download,
  Circle,
} from "lucide-react";
import {
  getFirmwareUpdateNotificationEnabled,
  setFirmwareUpdateNotificationEnabled,
} from "@/lib/firmware-update-checker";

// Notification preference types
interface NotificationPreferences {
  machineReady: boolean;
  waterEmpty: boolean;
  descaleDue: boolean;
  serviceDue: boolean;
  backflushDue: boolean;
  machineError: boolean;
  picoOffline: boolean;
  scheduleTriggered: boolean;
  brewComplete: boolean;
  firmwareUpdate: boolean;
}

const defaultPreferences: NotificationPreferences = {
  machineReady: true,
  waterEmpty: true,
  descaleDue: true,
  serviceDue: true,
  backflushDue: true,
  machineError: true,
  picoOffline: true,
  scheduleTriggered: true,
  brewComplete: false,
  firmwareUpdate: true,
};

// Notification categories for better UX
const notificationCategories = [
  {
    title: "Machine Status",
    description: "Get notified about your machine state",
    items: [
      {
        key: "machineReady" as const,
        label: "Machine Ready",
        description: "When your machine reaches brewing temperature",
        icon: Coffee,
        recommended: true,
      },
      {
        key: "brewComplete" as const,
        label: "Brew Complete",
        description: "When a brew finishes (brew-by-weight)",
        icon: Coffee,
        recommended: false,
      },
    ],
  },
  {
    title: "Alerts",
    description: "Important alerts that need attention",
    items: [
      {
        key: "waterEmpty" as const,
        label: "Water Tank Empty",
        description: "When the water tank needs refilling",
        icon: Droplet,
        recommended: true,
      },
      {
        key: "machineError" as const,
        label: "Machine Errors",
        description: "When something goes wrong with your machine",
        icon: AlertTriangle,
        recommended: true,
      },
      {
        key: "picoOffline" as const,
        label: "Control Board Offline",
        description: "When the control board stops responding",
        icon: Wifi,
        recommended: true,
      },
    ],
  },
  {
    title: "Maintenance",
    description: "Reminders for machine maintenance",
    items: [
      {
        key: "descaleDue" as const,
        label: "Descale Reminder",
        description: "When it's time to descale your machine",
        icon: Wrench,
        recommended: true,
      },
      {
        key: "serviceDue" as const,
        label: "Service Reminder",
        description: "When general service is recommended",
        icon: Wrench,
        recommended: true,
      },
      {
        key: "backflushDue" as const,
        label: "Backflush Reminder",
        description: "When it's time to backflush",
        icon: Wrench,
        recommended: true,
      },
    ],
  },
  {
    title: "Schedules",
    description: "Notifications related to schedules",
    items: [
      {
        key: "scheduleTriggered" as const,
        label: "Schedule Triggered",
        description: "When a schedule turns your machine on/off",
        icon: Clock,
        recommended: true,
      },
    ],
  },
  {
    title: "System",
    description: "System and firmware notifications",
    items: [
      {
        key: "firmwareUpdate" as const,
        label: "Firmware Updates",
        description:
          "When a new firmware version is available for your channel",
        icon: Download,
        recommended: true,
      },
    ],
  },
];

export function PushNotificationSettings() {
  const isDemo = isDemoMode();
  const {
    isSupported,
    isRegistered,
    isSubscribed,
    permission,
    subscribe,
    unsubscribe,
  } = usePushNotifications();

  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [preferences, setPreferences] =
    useState<NotificationPreferences>(defaultPreferences);
  const [loadingPrefs, setLoadingPrefs] = useState(false);
  const [savingPrefs, setSavingPrefs] = useState(false);

  const fetchPreferences = useCallback(async () => {
    // Get local firmware update preference
    const firmwareUpdateEnabled = getFirmwareUpdateNotificationEnabled();

    // Demo mode: use default preferences with local firmware setting
    if (isDemo) {
      setPreferences({
        ...defaultPreferences,
        firmwareUpdate: firmwareUpdateEnabled,
      });
      return;
    }

    setLoadingPrefs(true);
    try {
      const response = await fetch("/api/push/preferences", {
        credentials: "include",
      });
      if (response.ok) {
        const data = await response.json();
        // Merge server preferences with local firmware update preference
        // The ESP32 returns preferences directly at the top level
        setPreferences({
          machineReady: data.machineReady ?? defaultPreferences.machineReady,
          waterEmpty: data.waterEmpty ?? defaultPreferences.waterEmpty,
          descaleDue: data.descaleDue ?? defaultPreferences.descaleDue,
          serviceDue: data.serviceDue ?? defaultPreferences.serviceDue,
          backflushDue: data.backflushDue ?? defaultPreferences.backflushDue,
          machineError: data.machineError ?? defaultPreferences.machineError,
          picoOffline: data.picoOffline ?? defaultPreferences.picoOffline,
          scheduleTriggered: data.scheduleTriggered ?? defaultPreferences.scheduleTriggered,
          brewComplete: data.brewComplete ?? defaultPreferences.brewComplete,
          firmwareUpdate: firmwareUpdateEnabled,
        });
      } else {
        // If server fetch fails, at least set local preference
        setPreferences({
          ...defaultPreferences,
          firmwareUpdate: firmwareUpdateEnabled,
        });
      }
    } catch (err) {
      console.error("Failed to fetch notification preferences:", err);
      // Set defaults with local firmware preference on error
      setPreferences({
        ...defaultPreferences,
        firmwareUpdate: firmwareUpdateEnabled,
      });
    } finally {
      setLoadingPrefs(false);
    }
  }, [isDemo]);

  // Fetch notification preferences
  useEffect(() => {
    if (isSubscribed) {
      fetchPreferences();
    }
  }, [isSubscribed, fetchPreferences]);

  const updatePreference = async (
    key: keyof NotificationPreferences,
    value: boolean
  ) => {
    // Optimistically update UI
    setPreferences((prev) => ({ ...prev, [key]: value }));

    // Firmware update preference is stored locally (client-side periodic check)
    if (key === "firmwareUpdate") {
      setFirmwareUpdateNotificationEnabled(value);
      return;
    }

    // Demo mode: just update locally, no API call
    if (isDemo) {
      return;
    }

    setSavingPrefs(true);

    try {
      const response = await fetch("/api/push/preferences", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        credentials: "include",
        body: JSON.stringify({ [key]: value }),
      });

      if (!response.ok) {
        // Revert on error
        setPreferences((prev) => ({ ...prev, [key]: !value }));
        throw new Error("Failed to update preference");
      }
    } catch (err) {
      console.error("Failed to update notification preference:", err);
      setError("Failed to save preference");
      setTimeout(() => setError(null), 3000);
    } finally {
      setSavingPrefs(false);
    }
  };

  const handleSubscribe = async () => {
    setIsLoading(true);
    setError(null);
    try {
      const success = await subscribe();
      if (!success) {
        setError("Failed to subscribe to push notifications");
      } else {
        // Fetch preferences after subscribing
        fetchPreferences();
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to subscribe");
    } finally {
      setIsLoading(false);
    }
  };

  const handleUnsubscribe = async () => {
    setIsLoading(true);
    setError(null);
    try {
      const success = await unsubscribe();
      if (!success) {
        setError("Failed to unsubscribe from push notifications");
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to unsubscribe");
    } finally {
      setIsLoading(false);
    }
  };

  if (!isSupported) {
    return (
      <Card>
        <CardHeader>
          <CardTitle icon={<Bell className="w-5 h-5" />}>
            Push Notifications
          </CardTitle>
          <CardDescription>
            Push notifications are not supported in this browser
          </CardDescription>
        </CardHeader>
        <div className="p-6 text-center text-sm text-coffee-500">
          <BellOff className="w-12 h-12 mx-auto mb-3 text-coffee-300" />
          <p>Your browser does not support push notifications.</p>
        </div>
      </Card>
    );
  }

  return (
    <div className="space-y-6">
      {/* Main Push Notification Card */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Bell className="w-5 h-5" />}>
            Push Notifications
          </CardTitle>
          <CardDescription>
            Receive notifications on your device when your machine needs
            attention
          </CardDescription>
        </CardHeader>

        <div className="p-4 sm:p-6 space-y-5">
          {/* Status */}
          <div className="space-y-2.5">
            <div className="flex items-center justify-between">
              <span className="text-sm font-medium text-theme">
                Service Worker
              </span>
              <Badge variant={isRegistered ? "success" : "warning"}>
                {isRegistered ? (
                  <>
                    <Check className="w-3 h-3 mr-1" />
                    Registered
                  </>
                ) : (
                  <>
                    <X className="w-3 h-3 mr-1" />
                    Not Registered
                  </>
                )}
              </Badge>
            </div>

            <div className="flex items-center justify-between">
              <span className="text-sm font-medium text-theme">Permission</span>
              <Badge
                variant={
                  permission === "granted"
                    ? "success"
                    : permission === "denied"
                    ? "error"
                    : "warning"
                }
              >
                {permission === "granted" ? (
                  <>
                    <Check className="w-3 h-3 mr-1" />
                    Granted
                  </>
                ) : permission === "denied" ? (
                  <>
                    <X className="w-3 h-3 mr-1" />
                    Denied
                  </>
                ) : (
                  "Default"
                )}
              </Badge>
            </div>

            <div className="flex items-center justify-between">
              <span className="text-sm font-medium text-theme">
                Subscription
              </span>
              <Badge variant={isSubscribed ? "success" : "default"}>
                {isSubscribed ? (
                  <>
                    <Check className="w-3 h-3 mr-1" />
                    Active
                  </>
                ) : (
                  "Inactive"
                )}
              </Badge>
            </div>
          </div>

          {/* Error Message */}
          {error && (
            <div className="p-3 bg-error-soft border border-error rounded-lg flex items-start gap-2">
              <AlertCircle className="w-4 h-4 text-error mt-0.5 flex-shrink-0" />
              <p className="text-sm text-error">{error}</p>
            </div>
          )}

          {/* Actions */}
          <div className="space-y-3">
            {!isSubscribed ? (
              <Button
                onClick={handleSubscribe}
                loading={isLoading}
                disabled={permission === "denied" || !isRegistered}
                className="w-full"
              >
                <Bell className="w-4 h-4 mr-2" />
                Enable Push Notifications
              </Button>
            ) : (
              <Button
                onClick={handleUnsubscribe}
                loading={isLoading}
                variant="secondary"
                className="w-full"
              >
                <BellOff className="w-4 h-4 mr-2" />
                Disable Push Notifications
              </Button>
            )}

            {permission === "denied" && (
              <p className="text-xs text-coffee-500 text-center">
                Push notifications are blocked. Please enable them in your
                browser settings.
              </p>
            )}

            {!isRegistered && (
              <p className="text-xs text-coffee-500 text-center">
                Service worker is not registered. Please refresh the page.
              </p>
            )}
          </div>
        </div>
      </Card>

      {/* App Badge Card */}
      <AppBadgeSettings />

      {/* Notification Preferences Card - Always visible, disabled when not subscribed */}
      <Card className={!isSubscribed ? "opacity-60" : ""}>
        <CardHeader>
          <CardTitle icon={<Settings2 className="w-5 h-5" />}>
            Notification Preferences
            {savingPrefs && (
              <Loader2 className="w-4 h-4 ml-2 animate-spin inline" />
            )}
          </CardTitle>
          <CardDescription>
            {isSubscribed
              ? "Choose which notifications you want to receive"
              : "Enable push notifications above to configure preferences"}
          </CardDescription>
        </CardHeader>

        <div className="p-4 sm:p-6">
          {loadingPrefs ? (
            <div className="flex items-center justify-center py-8">
              <Loader2 className="w-6 h-6 animate-spin text-accent" />
            </div>
          ) : (
            <div className="space-y-6">
              {notificationCategories.map((category) => (
                <div key={category.title}>
                  <div className="mb-3">
                    <h3 className="font-semibold text-theme">
                      {category.title}
                    </h3>
                    <p className="text-xs text-theme-muted">
                      {category.description}
                    </p>
                  </div>
                  <div className="space-y-2">
                    {category.items.map((item) => {
                      const Icon = item.icon;
                      return (
                        <div
                          key={item.key}
                          className={`flex items-start gap-2.5 p-2.5 sm:p-3 bg-theme-secondary rounded-xl ${
                            !isSubscribed ? "pointer-events-none" : ""
                          }`}
                        >
                          <div className="p-1.5 sm:p-2 bg-theme-tertiary rounded-lg shrink-0">
                            <Icon className="w-4 h-4 text-accent" />
                          </div>
                          <div className="flex-1 min-w-0">
                            <div className="flex items-start justify-between gap-2">
                              <div className="min-w-0">
                                <div className="flex flex-wrap items-center gap-1.5">
                                  <span className="font-medium text-sm text-theme">
                                    {item.label}
                                  </span>
                                  {item.recommended && (
                                    <Badge
                                      variant="info"
                                      className="text-[10px] px-1.5 py-0"
                                    >
                                      Recommended
                                    </Badge>
                                  )}
                                </div>
                                <p className="text-xs text-theme-muted mt-0.5">
                                  {item.description}
                                </p>
                              </div>
                              <Toggle
                                checked={preferences[item.key]}
                                onChange={(checked) =>
                                  updatePreference(item.key, checked)
                                }
                                disabled={!isSubscribed}
                              />
                            </div>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </Card>
    </div>
  );
}

/**
 * App Badge Settings - Shows a dot on the app icon when machines are online
 */
function AppBadgeSettings() {
  const { isSupported } = useAppBadge();
  const showAppBadge = useStore((s) => s.preferences.showAppBadge);
  const setPreference = useStore((s) => s.setPreference);

  if (!isSupported) {
    return null; // Don't show if not supported
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Circle className="w-5 h-5" />}>
          App Badge
        </CardTitle>
        <CardDescription>
          Show a notification dot on the app icon
        </CardDescription>
      </CardHeader>

      <div className="p-4 sm:p-6">
        <div className="flex items-start gap-2.5 p-2.5 sm:p-3 bg-theme-secondary rounded-xl">
          <div className="p-1.5 sm:p-2 bg-theme-tertiary rounded-lg shrink-0">
            <Circle className="w-4 h-4 text-red-500 fill-red-500" />
          </div>
          <div className="flex-1 min-w-0">
            <div className="flex items-start justify-between gap-2">
              <div className="min-w-0">
                <span className="font-medium text-sm text-theme">
                  Machine Online Indicator
                </span>
                <p className="text-xs text-theme-muted mt-0.5">
                  Show a red dot on the app icon when any of your machines is powered on
                </p>
              </div>
              <Toggle
                checked={showAppBadge}
                onChange={(checked) => setPreference("showAppBadge", checked)}
              />
            </div>
          </div>
        </div>
        <p className="text-xs text-theme-muted mt-3 px-1">
          The badge appears on your home screen app icon when installed as a PWA.
        </p>
      </div>
    </Card>
  );
}
