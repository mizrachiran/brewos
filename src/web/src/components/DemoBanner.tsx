import { useState } from "react";
import { Play, X, LogOut } from "lucide-react";
import { useThemeStore } from "@/lib/themeStore";

interface DemoBannerProps {
  onExit?: () => void;
}

export function DemoBanner({ onExit }: DemoBannerProps) {
  const { theme } = useThemeStore();
  const isDark = theme.isDark;

  const [dismissed, setDismissed] = useState(() => {
    return sessionStorage.getItem("brewos-demo-banner-dismissed") === "true";
  });

  const handleExit = () => {
    if (onExit) {
      onExit();
    } else {
      // Remove demo param and reload
      const url = new URL(window.location.href);
      url.searchParams.delete("demo");
      window.location.href = url.pathname;
    }
  };

  const handleDismiss = () => {
    setDismissed(true);
    sessionStorage.setItem("brewos-demo-banner-dismissed", "true");
  };

  if (dismissed) {
    return null;
  }

  return (
    <div
      className={`
        border-b px-4 py-2.5
        ${
          isDark
            ? "bg-gradient-to-r from-violet-500/20 via-purple-500/15 to-violet-500/20 border-violet-400/30"
            : "bg-gradient-to-r from-violet-600/15 via-purple-600/10 to-violet-600/15 border-violet-500/40"
        }
      `}
    >
      <div className="max-w-6xl mx-auto flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div
            className={`
              flex items-center gap-2 px-2.5 py-1 rounded-full
              ${
                isDark
                  ? "bg-violet-500/25 border border-violet-400/30"
                  : "bg-violet-600/20 border border-violet-500/30"
              }
            `}
          >
            <Play
              className={`w-3.5 h-3.5 ${
                isDark
                  ? "text-violet-300 fill-violet-300"
                  : "text-violet-600 fill-violet-600"
              }`}
            />
            <span
              className={`text-xs font-bold uppercase tracking-wide ${
                isDark ? "text-violet-200" : "text-violet-700"
              }`}
            >
              Demo Mode
            </span>
          </div>
          <span
            className={`text-sm hidden sm:inline ${
              isDark ? "text-violet-300/90" : "text-violet-600/90"
            }`}
          >
            Explore BrewOS with simulated machine data
          </span>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={handleExit}
            className={`
              flex items-center gap-2 px-3 py-2 rounded-lg text-sm font-medium transition-colors
              ${
                isDark
                  ? "bg-violet-500/20 text-violet-200 hover:bg-violet-500/30 hover:text-violet-100 border border-violet-400/30"
                  : "bg-violet-600/15 text-violet-700 hover:bg-violet-600/25 hover:text-violet-800 border border-violet-500/30"
              }
            `}
          >
            <LogOut className="w-4 h-4" />
            <span>Exit Demo</span>
          </button>
          <button
            onClick={handleDismiss}
            className={`
              p-2 rounded-lg transition-colors
              ${
                isDark
                  ? "text-violet-300/70 hover:text-violet-100 hover:bg-violet-500/20"
                  : "text-violet-600/70 hover:text-violet-800 hover:bg-violet-500/15"
              }
            `}
            title="Dismiss banner"
            aria-label="Dismiss demo banner"
          >
            <X className="w-5 h-5" />
          </button>
        </div>
      </div>
    </div>
  );
}
