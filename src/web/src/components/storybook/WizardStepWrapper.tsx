import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { ProgressIndicator } from "@/components/wizard";
import { ArrowLeft, ArrowRight } from "lucide-react";
import type { ReactNode } from "react";
import { darkBgStyles } from "@/lib/darkBgStyles";

interface WizardStepWrapperProps {
  currentStep: number;
  steps: Array<{ id: string; title: string; icon: ReactNode }>;
  children: ReactNode;
  showBack?: boolean;
  backLabel?: string;
  nextLabel?: string;
  onBack?: () => void;
  onNext?: () => void;
  /** Force mobile or desktop layout for storybook */
  variant?: "auto" | "mobile" | "desktop";
}

export function WizardStepWrapper({
  currentStep,
  steps,
  children,
  showBack = true,
  backLabel = "Back",
  nextLabel = "Next",
  onBack,
  onNext,
  variant = "auto",
}: WizardStepWrapperProps) {
  const isWelcomeStep = steps[currentStep]?.id === "welcome";

  const renderNavigation = () => (
    <div className="flex justify-between pt-4 sm:pt-6 border-t border-theme mt-4 sm:mt-6">
      {showBack ? (
        <Button variant="ghost" onClick={onBack}>
          <ArrowLeft className="w-4 h-4" />
          {backLabel}
        </Button>
      ) : (
        <div />
      )}
      <Button onClick={onNext}>
        {nextLabel}
        <ArrowRight className="w-4 h-4" />
      </Button>
    </div>
  );

  // Mobile layout
  const mobileLayout = (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950">
      <div 
        className="min-h-screen flex flex-col px-5 py-6"
        style={darkBgStyles}
      >
        <ProgressIndicator steps={steps} currentStep={currentStep} />
        
        <div className={`flex-1 flex flex-col ${isWelcomeStep ? 'justify-center' : 'pt-4'}`}>
          <div className="animate-in fade-in slide-in-from-bottom-4 duration-500">
            {children}
            {renderNavigation()}
          </div>
        </div>
      </div>
    </div>
  );

  // Desktop layout
  const desktopLayout = (
    <div
      className={`min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 p-4 ${
        isWelcomeStep ? "flex items-center justify-center" : "flex justify-center"
      }`}
    >
      <div className={`w-full max-w-xl ${!isWelcomeStep ? "pt-16" : ""}`}>
        <ProgressIndicator steps={steps} currentStep={currentStep} />
        <Card>
          {children}
          {renderNavigation()}
        </Card>
      </div>
    </div>
  );

  if (variant === "mobile") {
    return mobileLayout;
  }

  if (variant === "desktop") {
    return desktopLayout;
  }

  // Auto: responsive layout
  return (
    <>
      {/* Mobile */}
      <div className="sm:hidden">
        {mobileLayout}
      </div>
      {/* Desktop */}
      <div className="hidden sm:block">
        {desktopLayout}
      </div>
    </>
  );
}
