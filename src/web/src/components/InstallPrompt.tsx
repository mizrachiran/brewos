import { useState } from 'react';
import { usePWAInstall } from '@/hooks/usePWAInstall';
import { Button } from './Button';
import { Card } from './Card';
import { Logo } from './Logo';
import { 
  Download, 
  X, 
  Share, 
  PlusSquare, 
  Check,
} from 'lucide-react';

interface InstallPromptProps {
  variant?: 'button' | 'banner' | 'card';
  className?: string;
  onInstalled?: () => void;
  onDismiss?: () => void;
  /** Force show even on desktop (for testing) */
  forceShow?: boolean;
}

export function InstallPrompt({ 
  variant = 'button', 
  className = '', 
  onInstalled,
  onDismiss,
  forceShow = false,
}: InstallPromptProps) {
  const { isInstallable, isInstalled, isIOS, promptInstall, isMobile, dismiss } = usePWAInstall();
  const [showIOSModal, setShowIOSModal] = useState(false);
  const [installing, setInstalling] = useState(false);
  const [isDismissed, setIsDismissed] = useState(false);

  // Don't show if already installed, not installable, dismissed, or on desktop (unless forced)
  if (isInstalled || !isInstallable || isDismissed) return null;
  
  // Only show on mobile unless forceShow is true
  if (!isMobile && !forceShow) return null;

  const handleDismiss = () => {
    setIsDismissed(true);
    dismiss();
    onDismiss?.();
  };

  const handleInstall = async () => {
    if (isIOS) {
      setShowIOSModal(true);
    } else {
      setInstalling(true);
      const success = await promptInstall();
      setInstalling(false);
      if (success) {
        onInstalled?.();
      }
    }
  };

  // Button variant
  if (variant === 'button') {
    return (
      <>
        <Button
          variant="secondary"
          size="sm"
          onClick={handleInstall}
          loading={installing}
          className={className}
        >
          <Download className="w-4 h-4" />
          Install App
        </Button>
        
        {showIOSModal && (
          <IOSInstallModal onClose={() => setShowIOSModal(false)} />
        )}
      </>
    );
  }

  // Banner variant
  if (variant === 'banner') {
    return (
      <>
        <div className={`bg-theme-card border border-theme rounded-xl p-4 flex items-center justify-between ${className}`}>
          <div className="flex items-center gap-3">
            <Logo size="sm" />
            <div>
              <p className="font-medium text-theme text-sm">Install BrewOS</p>
              <p className="text-xs text-theme-muted">
                Add to your home screen
              </p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <Button
              size="sm"
              onClick={handleInstall}
              loading={installing}
            >
              <Download className="w-4 h-4" />
              Install
            </Button>
            <button 
              onClick={handleDismiss}
              className="p-2 hover:bg-theme rounded-lg transition-colors"
              aria-label="Dismiss"
            >
              <X className="w-4 h-4 text-theme-muted" />
            </button>
          </div>
        </div>

        {showIOSModal && (
          <IOSInstallModal onClose={() => setShowIOSModal(false)} />
        )}
      </>
    );
  }

  // Card variant
  return (
    <>
      <Card className={`relative ${className}`}>
        {/* Dismiss button */}
        <button 
          onClick={handleDismiss}
          className="absolute top-3 right-3 p-1.5 hover:bg-theme rounded-lg transition-colors"
          aria-label="Dismiss"
        >
          <X className="w-4 h-4 text-theme-muted" />
        </button>
        
        <div className="text-center py-4">
          <div className="flex justify-center mb-4">
            <Logo size="md" />
          </div>
          <h3 className="text-lg font-semibold text-theme mb-1">
            Install BrewOS
          </h3>
          <p className="text-sm text-theme-muted mb-4 max-w-xs mx-auto">
            Add BrewOS to your home screen for quick access and push notifications.
          </p>
          <Button
            onClick={handleInstall}
            loading={installing}
            className="w-full max-w-xs"
          >
            <Download className="w-4 h-4" />
            Install App
          </Button>
        </div>
      </Card>

      {showIOSModal && (
        <IOSInstallModal onClose={() => setShowIOSModal(false)} />
      )}
    </>
  );
}

// iOS-specific installation instructions modal
function IOSInstallModal({ onClose }: { onClose: () => void }) {
  const isIPad = /ipad/.test(navigator.userAgent.toLowerCase());

  return (
    <div className="fixed inset-0 bg-black/60 backdrop-blur-sm z-50 flex items-end sm:items-center justify-center p-4">
      <div className="bg-theme-card rounded-2xl w-full max-w-sm overflow-hidden shadow-2xl animate-slide-up">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-theme">
          <h3 className="text-lg font-semibold text-theme">Install BrewOS</h3>
          <button 
            onClick={onClose}
            className="p-1 hover:bg-theme-card rounded-lg transition-colors"
          >
            <X className="w-5 h-5 text-theme-muted" />
          </button>
        </div>

        {/* Instructions */}
        <div className="p-6 space-y-6">
          <p className="text-sm text-theme-muted text-center">
            Install BrewOS on your {isIPad ? 'iPad' : 'iPhone'} for quick access
          </p>

          {/* Step 1 */}
          <div className="flex items-start gap-4">
            <div className="w-8 h-8 bg-accent/10 rounded-full flex items-center justify-center flex-shrink-0 mt-0.5">
              <span className="text-sm font-bold text-accent">1</span>
            </div>
            <div className="flex-1">
              <div className="flex items-center gap-2 mb-1">
                <p className="font-medium text-theme">Tap the Share button</p>
                <Share className="w-5 h-5 text-accent" />
              </div>
              <p className="text-sm text-theme-muted">
                {isIPad 
                  ? 'Located in the top toolbar' 
                  : 'Located at the bottom of your screen'}
              </p>
            </div>
          </div>

          {/* Step 2 */}
          <div className="flex items-start gap-4">
            <div className="w-8 h-8 bg-accent/10 rounded-full flex items-center justify-center flex-shrink-0 mt-0.5">
              <span className="text-sm font-bold text-accent">2</span>
            </div>
            <div className="flex-1">
              <div className="flex items-center gap-2 mb-1">
                <p className="font-medium text-theme">Add to Home Screen</p>
                <PlusSquare className="w-5 h-5 text-accent" />
              </div>
              <p className="text-sm text-theme-muted">
                Scroll down and tap "Add to Home Screen"
              </p>
            </div>
          </div>

          {/* Step 3 */}
          <div className="flex items-start gap-4">
            <div className="w-8 h-8 bg-emerald-500/10 rounded-full flex items-center justify-center flex-shrink-0 mt-0.5">
              <Check className="w-4 h-4 text-emerald-500" />
            </div>
            <div className="flex-1">
              <p className="font-medium text-theme mb-1">Tap "Add"</p>
              <p className="text-sm text-theme-muted">
                Confirm to add BrewOS to your home screen
              </p>
            </div>
          </div>

          {/* Arrow pointing down (for iPhones) */}
          {!isIPad && (
            <div className="flex flex-col items-center pt-2">
              <p className="text-xs text-theme-muted mb-2">Look for the Share button below</p>
              <div className="animate-bounce">
                <svg 
                  className="w-6 h-6 text-accent" 
                  fill="none" 
                  viewBox="0 0 24 24" 
                  stroke="currentColor"
                >
                  <path 
                    strokeLinecap="round" 
                    strokeLinejoin="round" 
                    strokeWidth={2} 
                    d="M19 14l-7 7m0 0l-7-7m7 7V3" 
                  />
                </svg>
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="p-4 bg-theme border-t border-theme">
          <Button 
            variant="secondary" 
            onClick={onClose}
            className="w-full"
          >
            Got it
          </Button>
        </div>
      </div>
    </div>
  );
}

// Export the hook for direct usage
export { usePWAInstall };

