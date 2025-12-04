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
    <div className="py-6 sm:py-8 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Logo with subtle animation */}
      <div className="flex justify-center mb-6 sm:mb-8">
        <div className="relative">
          <div className="absolute inset-0 bg-accent/20 rounded-full blur-2xl animate-pulse" />
          <div className="relative">
            <Logo size="md" className="sm:hidden" />
            <Logo size="lg" className="hidden sm:block" />
          </div>
        </div>
      </div>

      {/* Welcome heading */}
      <div className="text-center mb-6 sm:mb-8 px-4">
        <h1 className="text-2xl sm:text-3xl font-bold text-theme tracking-tight mb-2 sm:mb-3">
          Welcome to BrewOS
        </h1>
        <p className="text-theme-muted text-sm sm:text-base max-w-md mx-auto leading-relaxed">
          Control your espresso machine from anywhere. Let's add your first
          machine in just a few moments.
        </p>
      </div>

      {/* Action buttons */}
      <div className="space-y-3 sm:space-y-4 max-w-sm mx-auto mb-6 sm:mb-8 px-4">
        <Button
          className="w-full py-3.5 sm:py-4 text-sm sm:text-base font-semibold shadow-lg hover:shadow-xl transition-all duration-300 hover:scale-[1.02]"
          onClick={onScanClick}
        >
          <QrCode className="w-4 h-4 sm:w-5 sm:h-5" />
          Scan QR Code
          <ArrowRight className="w-4 h-4 sm:w-5 sm:h-5 ml-auto" />
        </Button>

        <div className="relative py-1">
          <div className="absolute inset-0 flex items-center">
            <div className="w-full border-t border-theme/20"></div>
          </div>
          <div className="relative flex justify-center text-xs uppercase">
            <span className="bg-theme-card px-3 text-theme-muted">or</span>
          </div>
        </div>

        <button
          className="w-full py-2.5 sm:py-3 text-theme-muted hover:text-theme transition-colors duration-200 font-medium flex items-center justify-center text-xs sm:text-sm"
          onClick={onManualClick}
        >
          Enter code manually
        </button>
      </div>

      {/* How it works */}
      <div className="border-t border-theme/10 pt-6 sm:pt-8 px-4">
        <div className="text-center mb-4 sm:mb-6">
          <h3 className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
            How it works
          </h3>
        </div>
        <div className="grid grid-cols-3 gap-2 sm:gap-4 max-w-md mx-auto">
          {steps.map((step, index) => {
            const Icon = step.icon;
            return (
              <div
                key={index}
                className="flex flex-col items-center gap-1.5 sm:gap-2 animate-in fade-in slide-in-from-bottom-4"
                style={{ animationDelay: `${index * 100}ms` }}
              >
                <div className="w-10 h-10 sm:w-12 sm:h-12 bg-accent/10 rounded-lg sm:rounded-xl flex items-center justify-center border border-accent/20 shadow-sm">
                  <Icon className="w-5 h-5 sm:w-6 sm:h-6 text-accent" />
                </div>
                <div className="text-center">
                  <p className="text-xs font-semibold text-theme mb-0.5 leading-tight">
                    {step.title}
                  </p>
                  <p className="text-xs text-theme-muted leading-tight">
                    {step.description}
                  </p>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Quick tip */}
      <div className="mt-4 sm:mt-6 text-center px-4">
        <p className="text-[10px] sm:text-xs text-theme-muted">
          💡 Find the QR code on your machine's display
        </p>
      </div>
    </div>
  );
}
