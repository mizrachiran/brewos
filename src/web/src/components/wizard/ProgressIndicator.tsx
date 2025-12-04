import { cloneElement, isValidElement } from "react";
import { Check } from "lucide-react";
import type { WizardStep } from "./types";

interface ProgressIndicatorProps {
  steps: WizardStep[];
  currentStep: number;
}

const iconClassName = "w-3.5 h-3.5 xs:w-5 xs:h-5";

export function ProgressIndicator({ steps, currentStep }: ProgressIndicatorProps) {
  return (
    <div className="flex justify-center mb-3 xs:mb-6">
      <div className="flex items-center gap-1 xs:gap-2">
        {steps.map((step, index) => (
          <div key={step.id} className="flex items-center">
            <div
              className={`w-7 h-7 xs:w-10 xs:h-10 rounded-full flex items-center justify-center transition-colors ${
                index < currentStep
                  ? "bg-emerald-500 text-white"
                  : index === currentStep
                  ? "bg-accent text-white"
                  : "bg-white/10 xs:bg-theme-tertiary text-white/50 xs:text-theme-muted"
              }`}
            >
              {index < currentStep ? (
                <Check className={iconClassName} />
              ) : isValidElement(step.icon) ? (
                cloneElement(step.icon, { className: iconClassName })
              ) : (
                step.icon
              )}
            </div>
            {index < steps.length - 1 && (
              <div
                className={`w-4 xs:w-8 h-0.5 mx-0.5 xs:mx-1 transition-colors ${
                  index < currentStep ? "bg-emerald-500" : "bg-white/10 xs:bg-theme-tertiary"
                }`}
              />
            )}
          </div>
        ))}
      </div>
    </div>
  );
}

