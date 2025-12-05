import { Check, Loader2, Sparkles, Coffee } from "lucide-react";

interface SuccessStepProps {
  deviceName?: string;
}

export function SuccessStep({ deviceName }: SuccessStepProps) {
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
        <div className="bg-white/5 xs:bg-theme-secondary rounded-lg xs:rounded-xl p-3 xs:p-5 border border-white/10 xs:border-theme/20">
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
