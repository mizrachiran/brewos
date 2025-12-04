import { Loader2 } from "lucide-react";

interface ConnectingViewProps {
  ssid: string;
}

export function ConnectingView({ ssid }: ConnectingViewProps) {
  return (
    <div className="text-center py-8">
      <Loader2 className="w-8 h-8 animate-spin text-accent mx-auto mb-4" />
      <p className="text-theme-secondary">Connecting to {ssid}...</p>
    </div>
  );
}

