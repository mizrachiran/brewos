import { Button } from "@/components/Button";
import { Logo } from "@/components/Logo";
import { QrCode, ArrowRight, Smartphone, CheckCircle } from "lucide-react";

interface WelcomeStepProps {
  onScanClick?: () => void;
  onManualClick?: () => void;
}

export function WelcomeStep({ onScanClick, onManualClick }: WelcomeStepProps) {
  const steps = [
    {
      icon: QrCode,
      title: "Get Code",
      description: "From machine display",
    },
    {
      icon: Smartphone,
      title: "Scan or Enter",
      description: "QR code or manual",
    },
    {
      icon: CheckCircle,
      title: "Add Machine",
      description: "Control remotely",
    },
  ];

  return (
    <div className="py-3 xs:py-8 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Logo with subtle animation */}
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
      <div className="text-center mb-4 xs:mb-8">
        <h1 className="text-xl xs:text-3xl font-bold text-theme tracking-tight mb-1 xs:mb-3">
          Welcome to BrewOS
        </h1>
        <p className="text-theme-muted text-xs xs:text-base max-w-md mx-auto leading-relaxed">
          Control your espresso machine from anywhere. Let's add your first
          machine in just a few moments.
        </p>
      </div>

      {/* Action buttons */}
      <div className="space-y-2 xs:space-y-4 max-w-sm mx-auto mb-4 xs:mb-8">
        <Button
          className="w-full py-3 xs:py-4 text-sm xs:text-base font-semibold shadow-lg hover:shadow-xl transition-all duration-300 hover:scale-[1.02]"
          onClick={onScanClick}
        >
          <QrCode className="w-4 h-4 xs:w-5 xs:h-5" />
          Scan QR Code
          <ArrowRight className="w-4 h-4 xs:w-5 xs:h-5 ml-auto" />
        </Button>

        <div className="relative py-1">
          <div className="absolute inset-0 flex items-center">
            <div className="w-full border-t border-white/10 xs:border-theme/20"></div>
          </div>
          <div className="relative flex justify-center text-xs uppercase">
            <span className="px-3 text-theme-muted">or</span>
          </div>
        </div>

        <button
          className="w-full py-2 xs:py-3 text-theme-muted hover:text-theme transition-colors duration-200 font-medium flex items-center justify-center text-xs xs:text-sm"
          onClick={onManualClick}
        >
          Enter code manually
        </button>
      </div>

      {/* How it works */}
      <div className="border-t border-white/10 xs:border-theme/10 pt-4 xs:pt-8">
        <div className="text-center mb-3 xs:mb-6">
          <h3 className="text-[10px] xs:text-xs font-semibold uppercase tracking-wider text-theme-muted">
            How it works
          </h3>
        </div>
        <div className="grid grid-cols-3 gap-2 xs:gap-4 max-w-md mx-auto">
          {steps.map((step, index) => {
            const Icon = step.icon;
            return (
              <div
                key={index}
                className="flex flex-col items-center gap-1 xs:gap-2 animate-in fade-in slide-in-from-bottom-4"
                style={{ animationDelay: `${index * 100}ms` }}
              >
                <div className="w-9 h-9 xs:w-12 xs:h-12 bg-accent/10 rounded-lg xs:rounded-xl flex items-center justify-center border border-accent/20 shadow-sm">
                  <Icon className="w-4 h-4 xs:w-6 xs:h-6 text-accent" />
                </div>
                <div className="text-center">
                  <p className="text-[10px] xs:text-xs font-semibold text-theme mb-0.5 leading-tight">
                    {step.title}
                  </p>
                  <p className="text-xs text-theme-muted leading-tight hidden xs:block">
                    {step.description}
                  </p>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Quick tip */}
      <div className="mt-3 xs:mt-6 text-center">
        <p className="text-[10px] xs:text-xs text-theme-muted">
          💡 Find the QR code on your machine's display
        </p>
      </div>
    </div>
  );
}
