import { Logo } from "@/components/Logo";

export function SetupHeader() {
  return (
    <div className="text-center mb-6">
      <div className="flex justify-center mb-4">
        <Logo size="lg" />
      </div>
      <h1 className="text-2xl font-bold text-theme">WiFi Setup</h1>
      <p className="text-theme-muted mt-1">Connect your BrewOS to WiFi</p>
    </div>
  );
}

