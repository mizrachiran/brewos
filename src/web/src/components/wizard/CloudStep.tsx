import { Button } from "@/components/Button";
import { Cloud, Loader2, Check, Copy } from "lucide-react";
import { QRCodeSVG } from "qrcode.react";
import type { PairingData } from "./types";

interface CloudStepProps {
  pairing: PairingData | null;
  loading: boolean;
  copied: boolean;
  onCopy: () => void;
  onSkip: () => void;
}

export function CloudStep({ pairing, loading, copied, onCopy, onSkip }: CloudStepProps) {
  return (
    <div className="py-6">
      <div className="text-center mb-6">
        <div className="w-16 h-16 bg-accent/10 rounded-2xl flex items-center justify-center mx-auto mb-4">
          <Cloud className="w-8 h-8 text-accent" />
        </div>
        <h2 className="text-2xl font-bold text-coffee-900 mb-2">Connect to Cloud</h2>
        <p className="text-coffee-500">Control your machine from anywhere (optional)</p>
      </div>

      <div className="bg-cream-100 rounded-2xl p-6 mb-6">
        <div className="flex flex-col items-center">
          {loading ? (
            <div className="w-48 h-48 flex items-center justify-center">
              <Loader2 className="w-8 h-8 animate-spin text-accent" />
            </div>
          ) : pairing ? (
            <PairingQRCode pairing={pairing} copied={copied} onCopy={onCopy} />
          ) : (
            <p className="text-coffee-400">Could not generate pairing code</p>
          )}
        </div>
      </div>

      <div className="text-center">
        <button
          onClick={onSkip}
          className="text-sm text-coffee-400 hover:text-coffee-600 hover:underline"
        >
          Skip for now — you can set this up later
        </button>
      </div>
    </div>
  );
}

interface PairingQRCodeProps {
  pairing: PairingData;
  copied: boolean;
  onCopy: () => void;
}

function PairingQRCode({ pairing, copied, onCopy }: PairingQRCodeProps) {
  return (
    <>
      <div className="bg-white p-3 rounded-xl mb-4">
        <QRCodeSVG value={pairing.url} size={160} level="M" />
      </div>
      <p className="text-sm text-coffee-500 mb-3">
        Scan with your phone or visit{" "}
        <a
          href="https://cloud.brewos.io"
          target="_blank"
          rel="noopener noreferrer"
          className="text-accent hover:underline"
        >
          cloud.brewos.io
        </a>
      </p>

      <div className="w-full max-w-xs">
        <div className="flex items-center gap-2">
          <code className="flex-1 bg-white px-3 py-2 rounded-lg text-xs font-mono text-coffee-600 text-center">
            {pairing.deviceId}:{pairing.token.substring(0, 8)}...
          </code>
          <Button variant="secondary" size="sm" onClick={onCopy}>
            {copied ? <Check className="w-4 h-4" /> : <Copy className="w-4 h-4" />}
          </Button>
        </div>
      </div>
    </>
  );
}

