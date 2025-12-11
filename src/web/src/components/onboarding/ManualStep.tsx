import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { ArrowRight, KeyRound, AlertCircle, Info } from "lucide-react";
import { useState, useEffect } from "react";

interface ManualStepProps {
  claimCode?: string;
  onClaimCodeChange?: (code: string) => void;
  error?: string;
  onBack?: () => void;
  onValidate?: () => void;
  disabled?: boolean;
  loading?: boolean;
}

export function ManualStep({
  claimCode,
  onClaimCodeChange,
  error,
  onBack,
  onValidate,
  disabled = false,
  loading = false,
}: ManualStepProps) {
  const isValidCode = claimCode && claimCode.length >= 8;

  // Detect mobile landscape
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia("(orientation: landscape) and (max-height: 500px)");
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) => setIsMobileLandscape(e.matches);
    handleChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleChange);
    return () => landscapeQuery.removeEventListener("change", handleChange);
  }, []);

  // Mobile landscape: two-column layout
  if (isMobileLandscape) {
    return (
      <div className="w-full max-w-4xl px-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
        <div className="flex gap-8 items-center">
          {/* Left column: Header + info */}
          <div className="flex-1 flex flex-col items-center">
            {/* Header */}
            <div className="text-center mb-4">
              <div className="w-14 h-14 bg-accent/10 rounded-2xl flex items-center justify-center mx-auto mb-3">
                <KeyRound className="w-7 h-7 text-accent" />
              </div>
              <h2 className="text-2xl font-bold text-theme mb-2">Enter Pairing Code</h2>
              <p className="text-theme-muted text-base">
                Find the code under <span className="font-semibold text-theme">System → Cloud Access</span>
              </p>
            </div>

            {/* Info box */}
            <div className="p-3 bg-accent/5 border border-accent/20 rounded-xl w-full max-w-[300px]">
              <div className="flex items-start gap-3">
                <Info className="w-5 h-5 text-accent flex-shrink-0 mt-0.5" />
                <div className="flex-1">
                  <p className="text-sm font-medium text-theme mb-1">Where to find your code</p>
                  <p className="text-xs text-theme-muted leading-relaxed">
                    Go to <span className="font-mono text-accent">System</span> → <span className="font-mono text-accent">Cloud Access</span>
                  </p>
                </div>
              </div>
            </div>
          </div>

          {/* Vertical divider */}
          <div className="w-px self-stretch min-h-[160px] bg-theme/10 flex-shrink-0" />

          {/* Right column: Input + buttons */}
          <div className="flex-1 flex flex-col items-center justify-center">
            <div className="w-full max-w-[260px] space-y-3">
              {/* Input */}
              <div>
                <Input
                  label="Pairing Code"
                  placeholder="X6ST-AP3G"
                  value={claimCode}
                  onChange={(e) =>
                    onClaimCodeChange?.(e.target.value.toUpperCase().replace(/[^A-Z0-9-]/g, ''))
                  }
                  maxLength={9}
                  className="text-center text-base font-mono tracking-wider"
                />
                {isValidCode && !error && (
                  <div className="mt-1 flex items-center gap-2 text-[10px] text-emerald-500">
                    <div className="w-1.5 h-1.5 rounded-full bg-emerald-500 animate-pulse" />
                    <span>Code format looks good</span>
                  </div>
                )}
              </div>

              {/* Error message */}
              {error && (
                <div className="p-2 rounded-lg bg-red-500/10 border border-red-500/20">
                  <div className="flex items-center gap-2">
                    <AlertCircle className="w-3.5 h-3.5 text-red-500 flex-shrink-0" />
                    <p className="text-[10px] text-red-500">{error}</p>
                  </div>
                </div>
              )}

              {/* Action buttons */}
              <div className="flex gap-3">
                {onBack && (
                  <Button variant="secondary" className="flex-1" onClick={onBack}>
                    Back
                  </Button>
                )}
                <Button
                  className={onBack ? "flex-1 font-semibold" : "w-full font-semibold"}
                  onClick={onValidate}
                  disabled={disabled || !isValidCode}
                  loading={loading}
                >
                  {loading ? "Validating..." : "Continue"}
                  <ArrowRight className="w-4 h-4" />
                </Button>
              </div>
            </div>
          </div>
        </div>
      </div>
    );
  }

  // Portrait layout
  return (
    <div className="py-3 xs:py-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
      {/* Header */}
      <div className="text-center mb-4 xs:mb-8">
        <div className="w-10 h-10 xs:w-14 xs:h-14 bg-accent/10 rounded-xl xs:rounded-2xl flex items-center justify-center mx-auto mb-2 xs:mb-4 border border-accent/20 shadow-sm">
          <KeyRound className="w-5 h-5 xs:w-7 xs:h-7 text-accent" />
        </div>
        <h2 className="text-xl xs:text-3xl font-bold text-theme mb-1 xs:mb-2">
          Enter Pairing Code
        </h2>
        <p className="text-theme-muted text-xs xs:text-base max-w-md mx-auto">
          Find the code on your BrewOS display under <span className="font-semibold text-theme">System → Cloud Access</span>
        </p>
      </div>

      {/* Info box */}
      <div className="mb-3 xs:mb-6 p-2.5 xs:p-4 bg-accent/5 border border-accent/20 rounded-lg xs:rounded-xl">
        <div className="flex items-start gap-2 xs:gap-3">
          <Info className="w-3.5 h-3.5 xs:w-5 xs:h-5 text-accent flex-shrink-0 mt-0.5" />
          <div className="flex-1">
            <p className="text-[10px] xs:text-sm font-medium text-theme mb-0.5 xs:mb-1">Where to find your code</p>
            <p className="text-[10px] xs:text-xs text-theme-muted leading-relaxed">
              Navigate to your machine's display, go to <span className="font-mono text-accent">System</span> → <span className="font-mono text-accent">Cloud Access</span> to find your 8-character pairing code.
            </p>
          </div>
        </div>
      </div>

      {/* Form */}
      <div className="space-y-3 xs:space-y-5">
        <div>
          <Input
            label="Pairing Code"
            placeholder="X6ST-AP3G"
            value={claimCode}
            onChange={(e) =>
              onClaimCodeChange?.(e.target.value.toUpperCase().replace(/[^A-Z0-9-]/g, ''))
            }
            hint="Enter the 8-character code (letters and numbers)"
            maxLength={9}
            className="text-center text-base xs:text-lg font-mono tracking-wider"
          />
          {isValidCode && !error && (
            <div className="mt-1.5 xs:mt-2 flex items-center gap-2 text-[10px] xs:text-xs text-emerald-500">
              <div className="w-1.5 h-1.5 rounded-full bg-emerald-500 animate-pulse" />
              <span>Code format looks good</span>
            </div>
          )}
        </div>

        {/* Error message */}
        {error && (
          <div className="p-2.5 xs:p-3 rounded-lg xs:rounded-xl bg-red-500/10 border border-red-500/20 animate-in fade-in slide-in-from-top-2">
            <div className="flex items-center gap-2">
              <AlertCircle className="w-3.5 h-3.5 xs:w-4 xs:h-4 text-red-500 flex-shrink-0" />
              <p className="text-[10px] xs:text-sm text-red-500">{error}</p>
            </div>
          </div>
        )}

        {/* Action buttons */}
        <div className="flex gap-2 xs:gap-3 pt-1 xs:pt-2">
          {onBack && (
            <Button variant="secondary" className="flex-1" onClick={onBack}>
              Back
            </Button>
          )}
          <Button
            className={onBack ? "flex-1 font-semibold" : "w-full font-semibold"}
            onClick={onValidate}
            disabled={disabled || !isValidCode}
            loading={loading}
          >
            {loading ? "Validating..." : "Continue"}
            <ArrowRight className="w-4 h-4" />
          </Button>
        </div>
      </div>
    </div>
  );
}
