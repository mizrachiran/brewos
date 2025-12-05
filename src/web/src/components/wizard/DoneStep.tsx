import { Check, Sparkles, Coffee, Settings, Scale, Globe } from "lucide-react";

interface DoneStepProps {
  machineName: string;
}

export function DoneStep({ machineName }: DoneStepProps) {
  const tips = [
    {
      icon: Globe,
      text: "Access locally at",
      highlight: "brewos.local",
    },
    {
      icon: Settings,
      text: "Adjust brew temperature in Settings",
    },
    {
      icon: Scale,
      text: "Connect a scale for brew-by-weight",
    },
  ];

  return (
    <div className="text-center py-4 xs:py-12 animate-in fade-in zoom-in-95 duration-700">
      {/* Success icon with animation */}
      <div className="relative mb-4 xs:mb-8">
        <div className="absolute inset-0 flex items-center justify-center">
          <div className="w-24 h-24 xs:w-40 xs:h-40 bg-success/20 rounded-full animate-ping opacity-75" />
        </div>
        <div className="relative w-16 h-16 xs:w-28 xs:h-28 bg-gradient-to-br from-success/20 to-success/10 rounded-full flex items-center justify-center mx-auto border-4 border-success/30">
          <div className="w-11 h-11 xs:w-20 xs:h-20 bg-success rounded-full flex items-center justify-center shadow-lg">
            <Check className="w-6 h-6 xs:w-12 xs:h-12 text-white animate-in zoom-in duration-500 delay-300" />
          </div>
        </div>
      </div>

      {/* Success message */}
      <div className="mb-4 xs:mb-8">
        <div className="inline-flex items-center gap-1.5 xs:gap-2 mb-1.5 xs:mb-3">
          <Sparkles className="w-4 h-4 xs:w-6 xs:h-6 text-accent animate-pulse" />
          <h2 className="text-xl xs:text-4xl font-bold text-theme">You're All Set!</h2>
        </div>
        <p className="text-theme-muted text-xs xs:text-lg max-w-md mx-auto leading-relaxed">
          Your <span className="font-semibold text-theme">{machineName || "machine"}</span> is ready to brew. Enjoy your perfect espresso!
        </p>
      </div>

      {/* Quick tips */}
      <div className="max-w-md mx-auto">
        <div className="bg-white/5 xs:bg-theme-secondary rounded-lg xs:rounded-xl p-3 xs:p-6 border border-white/10 xs:border-theme/20 shadow-lg">
          <div className="flex items-center gap-1.5 xs:gap-2 mb-2 xs:mb-4 justify-center">
            <Coffee className="w-4 h-4 xs:w-5 xs:h-5 text-accent" />
            <h3 className="font-semibold text-theme text-sm xs:text-base">Quick Tips</h3>
          </div>
          <ul className="text-[10px] xs:text-sm text-theme-muted text-left space-y-2 xs:space-y-3">
            {tips.map((tip, index) => {
              const Icon = tip.icon;
              return (
                <li
                  key={index}
                  className="flex items-start gap-2 xs:gap-3 animate-in fade-in slide-in-from-left-4"
                  style={{ animationDelay: `${index * 100}ms` }}
                >
                  <Icon className="w-3.5 h-3.5 xs:w-4 xs:h-4 text-accent flex-shrink-0 mt-0.5" />
                  <span>
                    {tip.text}{" "}
                    {tip.highlight && (
                      <span className="font-mono text-accent font-semibold">
                        {tip.highlight}
                      </span>
                    )}
                  </span>
                </li>
              );
            })}
          </ul>
        </div>
      </div>
    </div>
  );
}

