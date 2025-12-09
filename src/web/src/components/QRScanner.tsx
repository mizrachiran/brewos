import { useEffect, useRef, useState, useCallback } from "react";
import { Html5Qrcode, Html5QrcodeScannerState } from "html5-qrcode";
import { Camera, CameraOff, RefreshCw } from "lucide-react";
import { Button } from "./Button";

interface QRScannerProps {
  onScan: (result: string) => void;
  onError?: (error: string) => void;
  /** Compact mode for landscape - shows minimal error UI */
  compact?: boolean;
}

export function QRScanner({ onScan, onError, compact = false }: QRScannerProps) {
  const scannerRef = useRef<Html5Qrcode | null>(null);
  const [isScanning, setIsScanning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [hasPermission, setHasPermission] = useState<boolean | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  const startScanner = useCallback(async () => {
    if (!containerRef.current) return;

    setError(null);

    try {
      // Create scanner instance if not exists
      if (!scannerRef.current) {
        scannerRef.current = new Html5Qrcode("qr-reader");
      }

      // Check if already scanning
      if (scannerRef.current.getState() === Html5QrcodeScannerState.SCANNING) {
        return;
      }

      await scannerRef.current.start(
        { facingMode: "environment" },
        {
          fps: 10,
          qrbox: { width: 250, height: 250 },
          aspectRatio: 1,
        },
        (decodedText) => {
          // Stop scanning after successful read
          stopScanner();
          onScan(decodedText);
        },
        () => {
          // Ignore scan errors (no QR found yet)
        }
      );

      setIsScanning(true);
      setHasPermission(true);
    } catch (err) {
      const errorMessage =
        err instanceof Error ? err.message : "Failed to start camera";

      if (
        errorMessage.includes("Permission") ||
        errorMessage.includes("NotAllowed")
      ) {
        setHasPermission(false);
        setError("Camera permission denied. Please allow camera access.");
      } else if (errorMessage.includes("NotFound")) {
        setError("No camera found on this device.");
      } else {
        setError(errorMessage);
      }

      onError?.(errorMessage);
    }
  }, [onScan, onError]);

  const stopScanner = async () => {
    if (scannerRef.current) {
      try {
        if (
          scannerRef.current.getState() === Html5QrcodeScannerState.SCANNING
        ) {
          await scannerRef.current.stop();
        }
      } catch {
        // Ignore stop errors
      }
    }
    setIsScanning(false);
  };

  // Auto-start scanner on mount
  useEffect(() => {
    const timer = setTimeout(() => {
      startScanner();
    }, 100);

    return () => {
      clearTimeout(timer);
      stopScanner();
    };
  }, [startScanner]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (scannerRef.current) {
        try {
          scannerRef.current.clear();
        } catch {
          // Ignore cleanup errors
        }
        scannerRef.current = null;
      }
    };
  }, []);

  // Style the scanning box after it's created by the library
  useEffect(() => {
    if (!isScanning) return;

    const styleScanningBox = () => {
      // The library creates the scanning box dynamically
      // Try multiple selectors to find it
      const scanRegion = document.getElementById("qr-reader__scan_region");
      if (!scanRegion) return;

      // Find the scanning box div (usually the first absolutely positioned div)
      const scanningBox = scanRegion.querySelector<HTMLElement>(
        'div[style*="position"][style*="absolute"]'
      );

      if (scanningBox) {
        // Apply our custom styling
        scanningBox.style.border = "2px dashed var(--accent)";
        scanningBox.style.borderColor = "var(--accent)";
        scanningBox.style.opacity = "0.6";
        scanningBox.style.borderRadius = "1rem";
        scanningBox.style.boxShadow = "0 0 0 2px rgba(196, 112, 60, 0.1)";
      }

      // Also style SVG elements if present
      const svgElements =
        scanRegion.querySelectorAll<SVGElement>("svg rect, svg path");
      svgElements.forEach((el) => {
        el.style.stroke = "var(--accent)";
        el.style.strokeWidth = "2";
        el.style.strokeDasharray = "8 4";
        el.style.opacity = "0.7";
        el.style.fill = "none";
      });
    };

    // Try to style immediately and also after a short delay
    styleScanningBox();
    const timeout = setTimeout(styleScanningBox, 500);

    // Also watch for DOM changes
    const observer = new MutationObserver(styleScanningBox);
    const scanRegion = document.getElementById("qr-reader__scan_region");
    if (scanRegion) {
      observer.observe(scanRegion, { childList: true, subtree: true });
    }

    return () => {
      clearTimeout(timeout);
      observer.disconnect();
    };
  }, [isScanning]);

  return (
    <div className={compact ? "w-full" : "w-full max-w-xs mx-auto"} ref={containerRef}>
      {/* Scanner container - fixed aspect ratio to prevent size changes */}
      <div className="aspect-square w-full rounded-xl overflow-hidden bg-black relative">
        <div id="qr-reader" className="absolute inset-0 w-full h-full" />
      </div>

      {/* Error state */}
      {error && (
        compact ? (
          // Compact error for landscape - just icon and retry button
          <div className="mt-2 flex items-center justify-center gap-2">
            <CameraOff className="w-4 h-4 text-error" />
            <Button
              variant="secondary"
              size="sm"
              onClick={startScanner}
              className="text-xs"
            >
              <RefreshCw className="w-3 h-3" />
              Retry Camera
            </Button>
          </div>
        ) : (
          // Full error for portrait
          <div className="mt-3 p-3 bg-error-soft border border-error rounded-xl">
            <div className="flex items-center justify-between gap-3">
              <div className="flex items-center gap-2 flex-1 min-w-0">
                <CameraOff className="w-4 h-4 text-error flex-shrink-0" />
                <p className="text-xs sm:text-sm text-error truncate">{error}</p>
              </div>
              <Button
                variant="secondary"
                size="sm"
                onClick={startScanner}
                className="flex-shrink-0"
              >
                <RefreshCw className="w-3.5 h-3.5" />
                <span className="hidden sm:inline">Retry</span>
              </Button>
            </div>
          </div>
        )
      )}

      {/* Permission denied instructions - only in non-compact mode */}
      {hasPermission === false && !compact && (
        <div className="mt-4 p-4 bg-warning-soft border border-warning rounded-xl">
          <p className="text-sm text-warning mb-2 font-medium">
            To scan QR codes, allow camera access:
          </p>
          <ol className="text-xs text-warning space-y-1 list-decimal list-inside">
            <li>Click the camera/lock icon in your browser's address bar</li>
            <li>Allow camera permissions for this site</li>
            <li>Refresh the page or click "Try Again"</li>
          </ol>
        </div>
      )}

      {/* Scanning indicator */}
      {isScanning && !error && (
        <div className={`${compact ? "mt-2" : "mt-4"} flex items-center justify-center gap-2 text-sm text-theme-muted`}>
          <Camera className="w-4 h-4 animate-pulse" />
          <span>{compact ? "Scanning..." : "Point camera at QR code..."}</span>
        </div>
      )}
    </div>
  );
}
