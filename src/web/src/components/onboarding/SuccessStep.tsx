import { Check, Loader2, Sparkles, Coffee } from "lucide-react";
import { useState, useEffect } from "react";

interface SuccessStepProps {
  deviceName?: string;
}

export function SuccessStep({ deviceName }: SuccessStepProps) {
  // Detect mobile landscape
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia("(orientation: landscape) and (max-height: 500px)");
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) => setIsMobileLandscape(e.matches);
    handleChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleChange);
    return () => landscapeQuery.removeEventListener("change", handleChange);
  }, []);

  // Mobile landscape: two-column layout
  if (isMobileLandscape) {
    return (
      <div className="w-full max-w-4xl px-6 animate-in fade-in zoom-in-95 duration-700">
        <div className="flex gap-8 items-center">
          {/* Left column: Success icon + message */}
          <div className="flex-1 flex flex-col items-center justify-center">
            {/* Success icon */}
            <div className="relative mb-4">
              <div className="absolute inset-0 flex items-center justify-center">
                <div className="w-24 h-24 bg-success/20 rounded-full animate-ping opacity-75" />
              </div>
              <div className="relative w-20 h-20 bg-gradient-to-br from-success/20 to-success/10 rounded-full flex items-center justify-center border-4 border-success/30">
                <div className="w-14 h-14 bg-success rounded-full flex items-center justify-center shadow-lg">
                  <Check className="w-8 h-8 text-white animate-in zoom-in duration-500 delay-300" />
                </div>
              </div>
            </div>

            {/* Success message */}
            <div className="text-center">
              <div className="inline-flex items-center gap-2 mb-2">
                <Sparkles className="w-5 h-5 text-accent animate-pulse" />
                <h2 className="text-2xl font-bold text-theme">Machine Added!</h2>
              </div>
              <p className="text-theme-muted text-base leading-relaxed">
                <span className="font-semibold text-theme">{deviceName || "Your machine"}</span> is now connected.
              </p>
            </div>
          </div>

          {/* Vertical divider */}
          <div className="w-px self-stretch min-h-[140px] bg-theme/10 flex-shrink-0" />

          {/* Right column: Details + redirect */}
          <div className="flex-1 flex flex-col items-center justify-center">
            {/* Celebration details */}
            <div className="w-full max-w-[300px] mb-5">
              <div className="bg-theme-secondary rounded-xl p-5 border border-theme/20">
                <div className="flex items-center justify-center gap-2 mb-3">
                  <Coffee className="w-5 h-5 text-accent" />
                  <span className="font-semibold text-base text-theme">You're all set!</span>
                </div>
                <p className="text-sm text-theme-muted leading-relaxed text-center">
                  Your machine is ready to create the perfect espresso.
                </p>
              </div>
            </div>

            {/* Loading indicator */}
            <div className="flex items-center justify-center gap-2 text-sm text-theme-muted">
              <Loader2 className="w-4 h-4 animate-spin" />
              <span>Redirecting to dashboard...</span>
            </div>
          </div>
        </div>
      </div>
    );
  }

  // Portrait layout
  return (
    <div className="text-center py-4 xs:py-10 animate-in fade-in zoom-in-95 duration-700">
      {/* Success icon with animation */}
      <div className="relative mb-4 xs:mb-8">
        <div className="absolute inset-0 flex items-center justify-center">
          <div className="w-20 h-20 xs:w-32 xs:h-32 bg-success/20 rounded-full animate-ping opacity-75" />
        </div>
        <div className="relative w-16 h-16 xs:w-24 xs:h-24 bg-gradient-to-br from-success/20 to-success/10 rounded-full flex items-center justify-center mx-auto border-4 border-success/30">
          <div className="w-11 h-11 xs:w-16 xs:h-16 bg-success rounded-full flex items-center justify-center shadow-lg">
            <Check className="w-6 h-6 xs:w-10 xs:h-10 text-white animate-in zoom-in duration-500 delay-300" />
          </div>
        </div>
      </div>

      {/* Success message */}
      <div className="mb-3 xs:mb-6">
        <div className="inline-flex items-center gap-1.5 xs:gap-2 mb-1.5 xs:mb-3">
          <Sparkles className="w-4 h-4 xs:w-6 xs:h-6 text-accent animate-pulse" />
          <h2 className="text-xl xs:text-3xl font-bold text-theme">Machine Added!</h2>
        </div>
        <p className="text-theme-muted text-xs xs:text-base max-w-md mx-auto leading-relaxed">
          <span className="font-semibold text-theme">{deviceName || "Your machine"}</span> is now connected and ready to brew.
        </p>
      </div>

      {/* Celebration details */}
      <div className="max-w-sm mx-auto mb-4 xs:mb-8">
        <div className="bg-theme-secondary rounded-lg xs:rounded-xl p-3 xs:p-5 border border-theme/20">
          <div className="flex items-center justify-center gap-1.5 xs:gap-2 mb-1.5 xs:mb-3">
            <Coffee className="w-4 h-4 xs:w-5 xs:h-5 text-accent" />
            <span className="font-semibold text-xs xs:text-base text-theme">You're all set!</span>
          </div>
          <p className="text-[10px] xs:text-sm text-theme-muted leading-relaxed">
            Your machine is now accessible and ready to create the perfect espresso.
          </p>
        </div>
      </div>

      {/* Loading indicator */}
      <div className="flex items-center justify-center gap-2 text-[10px] xs:text-sm text-theme-muted animate-in fade-in delay-500">
        <Loader2 className="w-3.5 h-3.5 xs:w-4 xs:h-4 animate-spin" />
        <span>Redirecting to dashboard...</span>
      </div>
    </div>
  );
}
