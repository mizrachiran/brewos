import { useCallback } from 'react';
import { useStore } from './store';
import { getActiveConnection } from './connection';
import { useToast } from '@/components/Toast';
import { useConfirmDialog } from '@/components/ConfirmDialogProvider';

interface UseCommandOptions {
  /** Toast message to show on success */
  successMessage?: string;
  /** Toast message to show on error */
  errorMessage?: string;
  /** Skip connection check (for actions that work offline) */
  skipConnectionCheck?: boolean;
}

interface ConfirmOptions {
  /** Dialog title */
  title?: string;
  /** Dialog variant: warning, danger, or info */
  variant?: 'warning' | 'danger' | 'info';
  /** Custom confirm button text */
  confirmText?: string;
  /** Custom cancel button text */
  cancelText?: string;
}

/**
 * Hook for sending commands with built-in connection checking and error handling
 */
export function useCommand() {
  const connectionState = useStore((s) => s.connectionState);
  const { success, error } = useToast();
  const { confirm: showConfirmDialog } = useConfirmDialog();
  
  const isConnected = connectionState === 'connected';
  
  /**
   * Send a command to the ESP32 (or demo) with error handling
   */
  const sendCommand = useCallback((
    command: string, 
    payload?: Record<string, unknown>,
    options: UseCommandOptions = {}
  ): boolean => {
    const { 
      successMessage, 
      errorMessage = 'Action failed. Please try again.',
      skipConnectionCheck = false 
    } = options;
    
    // Check connection
    if (!skipConnectionCheck && !isConnected) {
      error('Not connected to machine');
      return false;
    }
    
    try {
      const connection = getActiveConnection();
      if (!connection) {
        error('No connection available');
        return false;
      }
      
      connection.sendCommand(command, payload);
      
      if (successMessage) {
        success(successMessage);
      }
      
      return true;
    } catch (err) {
      console.error(`Command ${command} failed:`, err);
      error(errorMessage);
      return false;
    }
  }, [isConnected, success, error]);
  
  /**
   * Send a command that requires user confirmation (async with custom dialog)
   */
  const sendCommandWithConfirm = useCallback(async (
    command: string,
    confirmMessage: string,
    payload?: Record<string, unknown>,
    options: UseCommandOptions & ConfirmOptions = {}
  ): Promise<boolean> => {
    const { title = 'Confirm Action', variant = 'warning', confirmText, cancelText, ...commandOptions } = options;
    
    const confirmed = await showConfirmDialog({
      title,
      description: confirmMessage,
      variant,
      confirmText,
      cancelText,
    });
    
    if (!confirmed) {
      return false;
    }
    return sendCommand(command, payload, commandOptions);
  }, [sendCommand, showConfirmDialog]);
  
  return {
    sendCommand,
    sendCommandWithConfirm,
    isConnected,
  };
}

