import { Logo } from "@/components/Logo";
import { Settings, Zap, Cloud } from "lucide-react";

export function WelcomeStep() {
  return (
    <div className="text-center py-8">
      <div className="flex justify-center mb-6">
        <Logo size="xl" />
      </div>
      <h1 className="text-3xl font-bold text-theme mb-3">Welcome to BrewOS</h1>
      <p className="text-theme-muted max-w-md mx-auto mb-8">
        Let's set up your espresso machine. This will only take a minute.
      </p>

      <div className="flex flex-col gap-4 text-sm text-theme-muted">
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 rounded-full bg-accent/10 flex items-center justify-center">
            <Settings className="w-4 h-4 text-accent" />
          </div>
          <span>Select your machine</span>
        </div>
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 rounded-full bg-accent/10 flex items-center justify-center">
            <Zap className="w-4 h-4 text-accent" />
          </div>
          <span>Configure power settings</span>
        </div>
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 rounded-full bg-accent/10 flex items-center justify-center">
            <Cloud className="w-4 h-4 text-accent" />
          </div>
          <span>Optional: Connect to cloud for remote access</span>
        </div>
      </div>
    </div>
  );
}
