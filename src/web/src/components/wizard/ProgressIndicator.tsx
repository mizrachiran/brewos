import { cloneElement } from "react";
import { Check } from "lucide-react";
import type { WizardStep } from "./types";

interface ProgressIndicatorProps {
  steps: WizardStep[];
  currentStep: number;
  /** Layout variant - horizontal (default) or vertical for landscape */
  variant?: "horizontal" | "vertical";
}

const iconClassName = "w-3 h-3 xs:w-5 xs:h-5";
const verticalIconClassName = "w-5 h-5";

export function ProgressIndicator({
  steps,
  currentStep,
  variant = "horizontal",
}: ProgressIndicatorProps) {
  // Vertical layout for landscape mode
  if (variant === "vertical") {
    return (
      <div className="flex flex-col items-center">
        {steps.map((step, index) => (
          <div key={step.id} className="flex flex-col items-center">
            {/* Step icon */}
            <div
              className={`w-9 h-9 rounded-full flex items-center justify-center transition-colors ${
                index < currentStep
                  ? "bg-emerald-500 text-white"
                  : index === currentStep
                  ? "bg-accent text-white"
                  : "bg-theme-tertiary text-theme-muted"
              }`}
            >
              {index < currentStep ? (
                <Check className={verticalIconClassName} />
              ) : (
                cloneElement(step.icon, { className: verticalIconClassName })
              )}
            </div>
            {/* Connecting line */}
            {index < steps.length - 1 && (
              <div
                className={`w-0.5 h-4 transition-colors ${
                  index < currentStep ? "bg-emerald-500" : "bg-theme-tertiary"
                }`}
              />
            )}
          </div>
        ))}
      </div>
    );
  }

  // Horizontal layout (default)
  return (
    <div className="flex justify-center mb-3 xs:mb-6 px-2">
      <div className="flex items-center gap-0.5 xs:gap-2">
        {steps.map((step, index) => (
          <div key={step.id} className="flex items-center">
            <div
              className={`w-6 h-6 xs:w-10 xs:h-10 rounded-full flex items-center justify-center transition-colors flex-shrink-0 ${
                index < currentStep
                  ? "bg-emerald-500 text-white"
                  : index === currentStep
                  ? "bg-accent text-white"
                  : "bg-white/10 xs:bg-theme-tertiary text-white/50 xs:text-theme-muted"
              }`}
            >
              {index < currentStep ? (
                <Check className={iconClassName} />
              ) : (
                cloneElement(step.icon, { className: iconClassName })
              )}
            </div>
            {index < steps.length - 1 && (
              <div
                className={`w-3 xs:w-8 h-0.5 mx-0.5 xs:mx-1 transition-colors flex-shrink-0 ${
                  index < currentStep
                    ? "bg-emerald-500"
                    : "bg-white/10 xs:bg-theme-tertiary"
                }`}
              />
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
