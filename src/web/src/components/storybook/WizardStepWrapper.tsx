import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { ProgressIndicator } from "@/components/wizard";
import { ArrowLeft, ArrowRight } from "lucide-react";
import type { ReactNode } from "react";

interface WizardStepWrapperProps {
  currentStep: number;
  steps: Array<{ id: string; title: string; icon: ReactNode }>;
  children: ReactNode;
  showBack?: boolean;
  backLabel?: string;
  nextLabel?: string;
  onBack?: () => void;
  onNext?: () => void;
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
}: WizardStepWrapperProps) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex items-center justify-center p-4">
      <div className="w-full max-w-xl">
        <ProgressIndicator steps={steps} currentStep={currentStep} />
        <Card>
          {children}
          <div className="flex justify-between pt-6 border-t border-theme mt-6">
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
        </Card>
      </div>
    </div>
  );
}

