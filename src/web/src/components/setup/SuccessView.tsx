import { Check } from "lucide-react";

export function SuccessView() {
  return (
    <div className="text-center py-8">
      <div className="w-16 h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
        <Check className="w-8 h-8 text-success" />
      </div>
      <h2 className="text-xl font-bold text-theme mb-2">Connected!</h2>
      <p className="text-theme-muted mb-4">
        Redirecting to <span className="font-mono text-accent">brewos.local</span>...
      </p>
    </div>
  );
}

