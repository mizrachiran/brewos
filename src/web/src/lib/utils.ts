import { type ClassValue, clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function formatTemp(temp: number): string {
  return temp.toFixed(1);
}

export function formatUptime(ms: number): string {
  const hours = Math.floor(ms / 3600000);
  const mins = Math.floor((ms % 3600000) / 60000);
  const secs = Math.floor((ms % 60000) / 1000);
  
  // Show seconds when under 1 hour for better visibility
  if (hours === 0) {
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  }
  return `${hours}:${mins.toString().padStart(2, '0')}`;
}

export function formatDuration(ms: number): string {
  const secs = Math.floor(ms / 1000);
  const mins = Math.floor(secs / 60);
  const remainingSecs = secs % 60;
  return `${mins}:${remainingSecs.toString().padStart(2, '0')}`;
}

export function formatTimeAgo(timestamp: number | null): string {
  if (!timestamp) return "—";
  
  const now = Date.now();
  const diff = now - timestamp;
  
  const minutes = Math.floor(diff / 60000);
  const hours = Math.floor(diff / 3600000);
  const days = Math.floor(diff / 86400000);
  
  if (minutes < 1) return "Just now";
  if (minutes < 60) return `${minutes}m ago`;
  if (hours < 24) return `${hours}h ago`;
  if (days === 1) return "Yesterday";
  return `${days}d ago`;
}

export function formatWeight(grams: number): string {
  return grams.toFixed(1);
}

export function formatBytes(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB'];
  let unitIndex = 0;
  let value = bytes;
  
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex++;
  }
  
  return `${value.toFixed(1)} ${units[unitIndex]}`;
}

export function getMachineStateLabel(state: string): string {
  const labels: Record<string, string> = {
    unknown: 'Unknown',
    init: 'Initializing',
    idle: 'Idle',
    heating: 'Heating',
    ready: 'Ready',
    brewing: 'Brewing',
    steaming: 'Steaming',
    cooldown: 'Cooling Down',
    fault: 'Fault',
  };
  return labels[state] || state;
}

export function getMachineStateColor(state: string): string {
  const colors: Record<string, string> = {
    unknown: 'bg-gray-100 text-gray-700',
    init: 'bg-blue-100 text-blue-700',
    idle: 'bg-cream-200 text-coffee-700',
    heating: 'bg-amber-100 text-amber-700',
    ready: 'bg-emerald-100 text-emerald-700',
    brewing: 'bg-accent/20 text-accent',
    steaming: 'bg-blue-100 text-blue-700',
    cooldown: 'bg-sky-100 text-sky-700',
    fault: 'bg-red-100 text-red-700',
  };
  return colors[state] || 'bg-gray-100 text-gray-700';
}

