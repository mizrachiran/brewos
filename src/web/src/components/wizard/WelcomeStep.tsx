import { Logo } from "@/components/Logo";
import { Settings, Zap, Cloud, Sparkles } from "lucide-react";

export function WelcomeStep() {
  const steps = [
    {
      icon: Settings,
      title: "Select your machine",
      description: "Choose from our supported models",
    },
    {
      icon: Zap,
      title: "Configure power settings",
      description: "Set voltage and current limits",
    },
    {
      icon: Cloud,
      title: "Connect to cloud (optional)",
      description: "Enable remote access from anywhere",
    },
  ];

  return (
    <div className="text-center py-4 xs:py-8 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Logo */}
      <div className="flex justify-center mb-4 xs:mb-8">
        <div className="relative">
          <div className="absolute inset-0 bg-accent/20 rounded-full blur-2xl animate-pulse" />
          <div className="relative">
            {/* Mobile: force light text for dark background */}
            <Logo size="lg" forceLight className="xs:hidden" />
            {/* Desktop: use theme colors */}
            <Logo size="xl" className="hidden xs:flex" />
          </div>
        </div>
      </div>

      {/* Welcome heading */}
      <div className="mb-5 xs:mb-10">
        <div className="inline-flex items-center gap-2 mb-2 xs:mb-3">
          <Sparkles className="w-4 h-4 xs:w-5 xs:h-5 text-accent animate-pulse" />
          <h1 className="text-2xl xs:text-4xl font-bold text-theme tracking-tight">
            Welcome to BrewOS
          </h1>
        </div>
        <p className="text-theme-muted text-sm xs:text-lg max-w-md mx-auto leading-relaxed">
          Let's set up your espresso machine. This will only take a minute.
        </p>
      </div>

      {/* Setup steps */}
      <div className="max-w-md mx-auto space-y-2 xs:space-y-4">
        {steps.map((step, index) => {
          const Icon = step.icon;
          return (
            <div
              key={index}
              className="flex items-start gap-3 xs:gap-4 p-3 xs:p-4 bg-white/5 xs:bg-theme-secondary/50 rounded-xl border border-white/10 xs:border-theme/10 hover:bg-white/10 xs:hover:bg-theme-secondary transition-colors duration-200 animate-in fade-in slide-in-from-left-4"
              style={{ animationDelay: `${index * 100}ms` }}
            >
              <div className="w-9 h-9 xs:w-10 xs:h-10 rounded-lg xs:rounded-xl bg-accent/10 flex items-center justify-center flex-shrink-0 border border-accent/20">
                <Icon className="w-4 h-4 xs:w-5 xs:h-5 text-accent" />
              </div>
              <div className="flex-1 text-left">
                <p className="font-semibold text-sm xs:text-base text-theme mb-0.5">{step.title}</p>
                <p className="text-xs xs:text-sm text-theme-muted">{step.description}</p>
              </div>
              <div className="flex-shrink-0">
                <div className="w-5 h-5 xs:w-6 xs:h-6 rounded-full bg-accent/10 flex items-center justify-center">
                  <span className="text-[10px] xs:text-xs font-bold text-accent">
                    {index + 1}
                  </span>
                </div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Quick tip */}
      <div className="mt-4 xs:mt-8 text-center">
        <p className="text-[10px] xs:text-xs text-theme-muted">
          💡 All settings can be changed later in Settings
        </p>
      </div>
    </div>
  );
}
