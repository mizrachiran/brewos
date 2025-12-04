import { Check, Loader2 } from "lucide-react";

interface SuccessStepProps {
  deviceName?: string;
}

export function SuccessStep({ deviceName }: SuccessStepProps) {
  return (
    <div className="text-center py-12">
      <div className="w-16 h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
        <Check className="w-8 h-8 text-success" />
      </div>
      <h2 className="text-2xl font-bold text-theme mb-2">Machine Added!</h2>
      <p className="text-theme-secondary mb-4">
        {deviceName || "Your machine"} is now connected to your account.
      </p>
      <div className="flex items-center justify-center gap-2 text-sm text-theme-muted">
        <Loader2 className="w-4 h-4 animate-spin" />
        Redirecting to dashboard...
      </div>
    </div>
  );
}

