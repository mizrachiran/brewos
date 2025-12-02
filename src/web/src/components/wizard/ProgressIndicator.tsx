import { Check } from "lucide-react";
import type { WizardStep } from "./types";

interface ProgressIndicatorProps {
  steps: WizardStep[];
  currentStep: number;
}

export function ProgressIndicator({ steps, currentStep }: ProgressIndicatorProps) {
  return (
    <div className="flex justify-center mb-6">
      <div className="flex items-center gap-2">
        {steps.map((step, index) => (
          <div key={step.id} className="flex items-center">
            <div
              className={`w-10 h-10 rounded-full flex items-center justify-center transition-colors ${
                index < currentStep
                  ? "bg-emerald-500 text-white"
                  : index === currentStep
                  ? "bg-accent text-white"
                  : "bg-cream-700/30 text-cream-400"
              }`}
            >
              {index < currentStep ? <Check className="w-5 h-5" /> : step.icon}
            </div>
            {index < steps.length - 1 && (
              <div
                className={`w-8 h-0.5 mx-1 transition-colors ${
                  index < currentStep ? "bg-emerald-500" : "bg-cream-700/30"
                }`}
              />
            )}
          </div>
        ))}
      </div>
    </div>
  );
}

