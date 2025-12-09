import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { ProgressIndicator } from "@/components/wizard";
import type { WizardStep } from "@/components/wizard/types";
import { ArrowLeft, ArrowRight } from "lucide-react";
import { useState, useEffect, type ReactNode } from "react";
import { darkBgStyles } from "@/lib/darkBgStyles";

interface WizardStepWrapperProps {
  currentStep: number;
  steps: WizardStep[];
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

  // Detect mobile landscape
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia(
      "(orientation: landscape) and (max-height: 500px)"
    );
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) =>
      setIsMobileLandscape(e.matches);
    handleChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleChange);
    return () => landscapeQuery.removeEventListener("change", handleChange);
  }, []);

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
        className="min-h-[100dvh] flex flex-col px-5 py-4 overflow-hidden"
        style={darkBgStyles}
      >
        <ProgressIndicator steps={steps} currentStep={currentStep} />
        
        <div className={`flex-1 flex flex-col overflow-y-auto ${isWelcomeStep ? 'justify-center' : 'pt-2'}`}>
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

  // Mobile landscape layout - two columns with steps on left
  const landscapeLayout = (
    <div className="h-[100dvh] bg-theme flex">
      {/* Left column: Progress steps - centered */}
      <div className="flex-shrink-0 flex items-center justify-center px-3">
        <ProgressIndicator
          steps={steps}
          currentStep={currentStep}
          variant="vertical"
        />
      </div>

      {/* Right column: Content fills remaining space */}
      <div className="flex-1 p-4 overflow-y-auto">
        <Card className="h-full flex flex-col animate-in fade-in slide-in-from-bottom-4 duration-300">
          <div className="flex-1 overflow-y-auto py-2">
            {children}
          </div>
          <div className="flex-shrink-0 flex justify-between pt-3 border-t border-theme mt-auto">
            {showBack ? (
              <Button variant="ghost" size="sm" onClick={onBack}>
                <ArrowLeft className="w-4 h-4" />
                {backLabel}
              </Button>
            ) : (
              <div />
            )}
            <Button size="sm" onClick={onNext}>
              {nextLabel}
              <ArrowRight className="w-4 h-4" />
            </Button>
          </div>
        </Card>
      </div>
    </div>
  );

  if (variant === "mobile") {
    return isMobileLandscape ? landscapeLayout : mobileLayout;
  }

  if (variant === "desktop") {
    return desktopLayout;
  }

  // Auto: responsive layout with landscape detection
  if (isMobileLandscape) {
    return landscapeLayout;
  }

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
