import { Coffee, QrCode, Wifi } from "lucide-react";

export function HowItWorks() {
  return (
    <div className="mt-8 text-center">
      <h3 className="text-sm font-semibold text-cream-300 mb-4">
        How it works
      </h3>
      <div className="grid grid-cols-3 gap-4 text-cream-400 text-xs">
        <div className="flex flex-col items-center gap-2">
          <div className="w-10 h-10 bg-cream-800/30 rounded-full flex items-center justify-center">
            <Wifi className="w-5 h-5" />
          </div>
          <span>Connect machine to WiFi</span>
        </div>
        <div className="flex flex-col items-center gap-2">
          <div className="w-10 h-10 bg-cream-800/30 rounded-full flex items-center justify-center">
            <QrCode className="w-5 h-5" />
          </div>
          <span>Scan QR code on display</span>
        </div>
        <div className="flex flex-col items-center gap-2">
          <div className="w-10 h-10 bg-cream-800/30 rounded-full flex items-center justify-center">
            <Coffee className="w-5 h-5" />
          </div>
          <span>Control from anywhere</span>
        </div>
      </div>
    </div>
  );
}

