/**
 * Shared formatting utilities
 */

/**
 * Format seconds into a human-readable runtime string
 */
export function formatRuntime(seconds: number): string {
  if (seconds <= 0) return "—";
  if (seconds < 3600) {
    return `${Math.floor(seconds / 60)}m`;
  } else if (seconds < 86400) {
    return `${Math.floor(seconds / 3600)}h ${Math.floor((seconds % 3600) / 60)}m`;
  } else {
    return `${Math.floor(seconds / 86400)}d ${Math.floor((seconds % 86400) / 3600)}h`;
  }
}

/**
 * Format milliseconds to seconds with 1 decimal
 */
export function formatMsToSeconds(ms: number): string | null {
  if (ms <= 0) return null;
  return (ms / 1000).toFixed(1);
}

