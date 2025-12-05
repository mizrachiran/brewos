import { useEffect } from 'react';
import { X, AlertTriangle, AlertCircle, Info } from 'lucide-react';
import { cn } from '@/lib/utils';
import { Button } from './Button';

interface ConfirmDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onConfirm: () => void;
  title: string;
  description?: string;
  children?: React.ReactNode;
  confirmText?: string;
  cancelText?: string;
  variant?: 'warning' | 'danger' | 'info';
  confirmLoading?: boolean;
}

export function ConfirmDialog({
  isOpen,
  onClose,
  onConfirm,
  title,
  description,
  children,
  confirmText = 'Confirm',
  cancelText = 'Cancel',
  variant = 'warning',
  confirmLoading = false,
}: ConfirmDialogProps) {
  // Handle keyboard events
  useEffect(() => {
    if (!isOpen) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, onClose]);

  if (!isOpen) return null;

  const variantStyles = {
    warning: {
      icon: <AlertTriangle className="w-6 h-6" />,
      iconBg: 'bg-amber-500/20 text-amber-500',
      confirmButton: 'primary' as const,
    },
    danger: {
      icon: <AlertCircle className="w-6 h-6" />,
      iconBg: 'bg-red-500/20 text-red-500',
      confirmButton: 'danger' as const,
    },
    info: {
      icon: <Info className="w-6 h-6" />,
      iconBg: 'bg-blue-500/20 text-blue-500',
      confirmButton: 'primary' as const,
    },
  };

  const styles = variantStyles[variant];

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center xs:p-4"
      onClick={onClose}
    >
      {/* Backdrop */}
      <div className="absolute inset-0 bg-black/50 xs:backdrop-blur-sm transition-opacity" />

      {/* Modal */}
      <div
        className="relative w-full h-full xs:h-auto xs:max-w-md bg-theme-card rounded-none xs:rounded-2xl xs:shadow-2xl border-0 xs:border border-theme overflow-hidden transform transition-all animate-in fade-in zoom-in-95 duration-200 flex flex-col"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-start gap-4 p-5 sm:p-6 flex-1 xs:flex-initial">
          {/* Icon */}
          <div
            className={cn(
              'w-12 h-12 rounded-xl flex items-center justify-center flex-shrink-0',
              styles.iconBg
            )}
          >
            {styles.icon}
          </div>

          {/* Title & Description */}
          <div className="flex-1 min-w-0 pt-1">
            <h2 className="text-lg sm:text-xl font-bold text-theme">{title}</h2>
            {description && (
              <p className="text-sm text-theme-muted mt-1">{description}</p>
            )}
          </div>

          {/* Close button */}
          <button
            onClick={onClose}
            className="p-2 -mt-1 -mr-2 rounded-lg hover:bg-theme-secondary text-theme-muted hover:text-theme transition-colors flex-shrink-0"
            aria-label="Close"
          >
            <X className="w-5 h-5" />
          </button>
        </div>

        {/* Content */}
        {children && (
          <div className="px-5 sm:px-6 pb-2">
            {children}
          </div>
        )}

        {/* Actions */}
        <div className="flex items-center justify-end gap-3 p-5 sm:p-6 pt-4 border-t border-theme mt-auto xs:mt-0">
          <Button variant="secondary" onClick={onClose} disabled={confirmLoading}>
            {cancelText}
          </Button>
          <Button
            variant={styles.confirmButton}
            onClick={onConfirm}
            loading={confirmLoading}
          >
            {confirmText}
          </Button>
        </div>
      </div>
    </div>
  );
}

