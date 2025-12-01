import { useState } from 'react';
import { useAppStore } from '@/lib/mode';
import { usePushNotifications } from '@/hooks/usePushNotifications';
import { Card, CardHeader, CardTitle, CardDescription } from '@/components/Card';
import { Button } from '@/components/Button';
import { Badge } from '@/components/Badge';
import { Bell, BellOff, Check, X, AlertCircle } from 'lucide-react';

export function PushNotificationSettings() {
  const { mode } = useAppStore();
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

  // Only show in cloud mode
  if (mode !== 'cloud') {
    return null;
  }

  const handleSubscribe = async () => {
    setIsLoading(true);
    setError(null);
    try {
      const success = await subscribe();
      if (!success) {
        setError('Failed to subscribe to push notifications');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to subscribe');
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
        setError('Failed to unsubscribe from push notifications');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to unsubscribe');
    } finally {
      setIsLoading(false);
    }
  };

  if (!isSupported) {
    return (
      <Card>
        <CardHeader>
          <CardTitle>Push Notifications</CardTitle>
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
    <Card>
      <CardHeader>
        <CardTitle>Push Notifications</CardTitle>
        <CardDescription>
          Receive notifications on your device when your machine needs attention
        </CardDescription>
      </CardHeader>

      <div className="p-6 space-y-6">
        {/* Status */}
        <div className="space-y-3">
          <div className="flex items-center justify-between">
            <span className="text-sm font-medium text-coffee-700">Service Worker</span>
            <Badge variant={isRegistered ? 'success' : 'warning'}>
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
            <span className="text-sm font-medium text-coffee-700">Permission</span>
            <Badge
              variant={
                permission === 'granted'
                  ? 'success'
                  : permission === 'denied'
                    ? 'error'
                    : 'warning'
              }
            >
              {permission === 'granted' ? (
                <>
                  <Check className="w-3 h-3 mr-1" />
                  Granted
                </>
              ) : permission === 'denied' ? (
                <>
                  <X className="w-3 h-3 mr-1" />
                  Denied
                </>
              ) : (
                'Default'
              )}
            </Badge>
          </div>

          <div className="flex items-center justify-between">
            <span className="text-sm font-medium text-coffee-700">Subscription</span>
            <Badge variant={isSubscribed ? 'success' : 'default'}>
              {isSubscribed ? (
                <>
                  <Check className="w-3 h-3 mr-1" />
                  Active
                </>
              ) : (
                'Inactive'
              )}
            </Badge>
          </div>
        </div>

        {/* Error Message */}
        {error && (
          <div className="p-3 bg-red-50 border border-red-200 rounded-lg flex items-start gap-2">
            <AlertCircle className="w-4 h-4 text-red-600 mt-0.5 flex-shrink-0" />
            <p className="text-sm text-red-700">{error}</p>
          </div>
        )}

        {/* Actions */}
        <div className="space-y-3">
          {!isSubscribed ? (
            <Button
              onClick={handleSubscribe}
              loading={isLoading}
              disabled={permission === 'denied' || !isRegistered}
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

          {permission === 'denied' && (
            <p className="text-xs text-coffee-500 text-center">
              Push notifications are blocked. Please enable them in your browser settings.
            </p>
          )}

          {!isRegistered && (
            <p className="text-xs text-coffee-500 text-center">
              Service worker is not registered. Please refresh the page.
            </p>
          )}
        </div>

        {/* Info */}
        <div className="pt-4 border-t border-coffee-200">
          <p className="text-xs text-coffee-500">
            You'll receive notifications for:
          </p>
          <ul className="text-xs text-coffee-500 mt-2 space-y-1 list-disc list-inside">
            <li>Machine ready to brew</li>
            <li>Water tank empty</li>
            <li>Maintenance reminders (descale, service, backflush)</li>
            <li>Machine errors</li>
            <li>Control board offline</li>
          </ul>
        </div>
      </div>
    </Card>
  );
}

