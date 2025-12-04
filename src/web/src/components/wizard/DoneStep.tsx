import { Check } from "lucide-react";

interface DoneStepProps {
  machineName: string;
}

export function DoneStep({ machineName }: DoneStepProps) {
  return (
    <div className="text-center py-12">
      <div className="w-20 h-20 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-6">
        <Check className="w-10 h-10 text-success" />
      </div>
      <h2 className="text-3xl font-bold text-theme mb-3">You're All Set!</h2>
      <p className="text-theme-muted max-w-sm mx-auto mb-8">
        Your {machineName || "machine"} is ready to brew. Enjoy your perfect espresso!
      </p>

      <div className="bg-theme-secondary rounded-xl p-4 max-w-sm mx-auto">
        <h3 className="font-semibold text-theme mb-2">Quick Tips:</h3>
        <ul className="text-sm text-theme-muted text-left space-y-1">
          <li>
            • Access locally at <span className="font-mono text-accent">brewos.local</span>
          </li>
          <li>• Adjust brew temperature in Settings</li>
          <li>• Connect a scale for brew-by-weight</li>
        </ul>
      </div>
    </div>
  );
}

