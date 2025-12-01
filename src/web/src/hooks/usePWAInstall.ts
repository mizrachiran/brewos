import { useState, useEffect, useCallback } from 'react';

interface BeforeInstallPromptEvent extends Event {
  prompt: () => Promise<void>;
  userChoice: Promise<{ outcome: 'accepted' | 'dismissed' }>;
}

interface RelatedApplication {
  platform: string;
  url?: string;
  id?: string;
}

interface NavigatorWithRelatedApps extends Navigator {
  getInstalledRelatedApps?: () => Promise<RelatedApplication[]>;
}

const DISMISSED_KEY = 'pwa-install-dismissed';

interface PWAInstallState {
  isInstallable: boolean;
  isInstalled: boolean;
  isIOS: boolean;
  isAndroid: boolean;
  isMobile: boolean;
  promptInstall: () => Promise<boolean>;
  dismiss: () => void;
}

export function usePWAInstall(): PWAInstallState {
  const [deferredPrompt, setDeferredPrompt] = useState<BeforeInstallPromptEvent | null>(null);
  const [isInstalled, setIsInstalled] = useState(false);
  const [isDismissed, setIsDismissed] = useState(() => {
    if (typeof window === 'undefined') return false;
    return localStorage.getItem(DISMISSED_KEY) === 'true';
  });

  // Detect platform
  const userAgent = typeof navigator !== 'undefined' ? navigator.userAgent.toLowerCase() : '';
  const isIOS = /iphone|ipad|ipod/.test(userAgent);
  const isAndroid = /android/.test(userAgent);
  const isMobile = isIOS || isAndroid || /mobile/.test(userAgent);

  // Check if already installed (standalone mode or via getInstalledRelatedApps)
  useEffect(() => {
    const checkInstalled = async () => {
      // Check for standalone mode (running as PWA)
      const isStandalone = 
        window.matchMedia('(display-mode: standalone)').matches ||
        (window.navigator as unknown as { standalone?: boolean }).standalone === true ||
        document.referrer.includes('android-app://');
      
      if (isStandalone) {
        setIsInstalled(true);
        return;
      }

      // Check using getInstalledRelatedApps API (Chrome 80+)
      const nav = navigator as NavigatorWithRelatedApps;
      if (nav.getInstalledRelatedApps) {
        try {
          const relatedApps = await nav.getInstalledRelatedApps();
          if (relatedApps.length > 0) {
            setIsInstalled(true);
            return;
          }
        } catch {
          // API not available or failed
        }
      }

      setIsInstalled(false);
    };

    checkInstalled();

    // Listen for display mode changes
    const mediaQuery = window.matchMedia('(display-mode: standalone)');
    mediaQuery.addEventListener('change', () => checkInstalled());

    return () => mediaQuery.removeEventListener('change', () => checkInstalled());
  }, []);

  // Capture beforeinstallprompt event (Android/Chrome)
  useEffect(() => {
    const handleBeforeInstallPrompt = (e: Event) => {
      e.preventDefault();
      setDeferredPrompt(e as BeforeInstallPromptEvent);
    };

    window.addEventListener('beforeinstallprompt', handleBeforeInstallPrompt);

    // Check if app was installed
    window.addEventListener('appinstalled', () => {
      setIsInstalled(true);
      setDeferredPrompt(null);
    });

    return () => {
      window.removeEventListener('beforeinstallprompt', handleBeforeInstallPrompt);
    };
  }, []);

  // Trigger install prompt (Android only)
  const promptInstall = useCallback(async (): Promise<boolean> => {
    if (!deferredPrompt) return false;

    try {
      await deferredPrompt.prompt();
      const { outcome } = await deferredPrompt.userChoice;
      
      if (outcome === 'accepted') {
        setIsInstalled(true);
        setDeferredPrompt(null);
        return true;
      }
    } catch (error) {
      console.error('Install prompt error:', error);
    }

    return false;
  }, [deferredPrompt]);

  // Dismiss the install prompt (user doesn't want to be asked again)
  const dismiss = useCallback(() => {
    setIsDismissed(true);
    localStorage.setItem(DISMISSED_KEY, 'true');
  }, []);

  return {
    isInstallable: !isInstalled && !isDismissed && (!!deferredPrompt || isIOS),
    isInstalled,
    isIOS,
    isAndroid,
    isMobile,
    promptInstall,
    dismiss,
  };
}

