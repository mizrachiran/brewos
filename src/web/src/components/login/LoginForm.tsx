import { Logo, LogoIcon } from "@/components/Logo";
import { AlertCircle, Wifi, ChevronRight } from "lucide-react";

// External links for legal pages (hosted on brewos.io)
const LEGAL_LINKS = {
  terms: "https://brewos.io/terms",
  privacy: "https://brewos.io/privacy",
} as const;

// Mock Google button for stories
function MockGoogleButton() {
  return (
    <button className="flex items-center gap-3 px-8 py-3 bg-white border border-gray-200 rounded-full shadow-sm hover:shadow-md transition-all hover:scale-[1.02]">
      <svg className="w-5 h-5" viewBox="0 0 24 24">
        <path
          fill="#4285F4"
          d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"
        />
        <path
          fill="#34A853"
          d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"
        />
        <path
          fill="#FBBC05"
          d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"
        />
        <path
          fill="#EA4335"
          d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"
        />
      </svg>
      <span className="text-gray-700 font-medium">Continue with Google</span>
    </button>
  );
}

interface LoginFormProps {
  /** Error message to display */
  error?: string;
  /** Enable entrance animations */
  animated?: boolean;
  /** Show mobile logo (for standalone mobile view) */
  showMobileLogo?: boolean;
  /** Custom Google button slot (for real GoogleLogin component) */
  googleButton?: React.ReactNode;
}

export function LoginForm({
  error,
  animated = true,
  showMobileLogo = true,
  googleButton,
}: LoginFormProps) {
  return (
    <div
      className={`w-full max-w-md relative z-10 ${
        animated ? "login-card-enter" : ""
      }`}
    >
      {/* Mobile logo (hidden on desktop) */}
      {showMobileLogo && (
        <div className="lg:hidden flex justify-center mb-4 sm:mb-8">
          <Logo size="lg" />
        </div>
      )}

      {/* Card */}
      <div className="bg-theme-card rounded-2xl sm:rounded-3xl p-5 sm:p-8 lg:p-10 shadow-2xl border border-theme">
        {/* Header */}
        <div className="text-center mb-5 sm:mb-8">
          <div className="inline-flex items-center justify-center w-12 h-12 sm:w-16 sm:h-16 rounded-xl sm:rounded-2xl bg-accent shadow-lg shadow-accent/20 mb-3 sm:mb-5">
            <LogoIcon size="md" className="filter brightness-0 invert" />
          </div>
          <h2 className="text-xl sm:text-2xl lg:text-3xl font-bold text-theme mb-1 sm:mb-2">
            Welcome back
          </h2>
          <p className="text-sm sm:text-base text-theme-muted mb-2">
            Sign in to manage your machines
          </p>
        </div>

        {/* Error message */}
        {error && (
          <div
            className={`mb-6 p-4 rounded-xl flex items-start gap-3 bg-error-soft border border-error ${
              animated ? "login-error" : ""
            }`}
          >
            <AlertCircle className="w-5 h-5 text-error flex-shrink-0 mt-0.5" />
            <span className="text-sm text-error">{error}</span>
          </div>
        )}

        {/* Google Sign In */}
        <div className={animated ? "login-google-btn" : ""}>
          <div className="flex justify-center mb-4 sm:mb-6">
            <div className="transform hover:scale-[1.02] transition-transform duration-200">
              {googleButton || <MockGoogleButton />}
            </div>
          </div>

          {/* Divider */}
          <div className="relative my-5 sm:my-8">
            <div className="absolute inset-0 flex items-center">
              <div className="w-full border-t border-theme" />
            </div>
            <div className="relative flex justify-center">
              <span className="px-3 sm:px-4 text-xs sm:text-sm text-theme-muted bg-theme-card">
                or connect locally
              </span>
            </div>
          </div>

          {/* Local connection option */}
          <a
            href="http://brewos.local"
            className="group flex items-center justify-between w-full p-3 sm:p-4 rounded-xl bg-theme-secondary hover:bg-theme-tertiary transition-all duration-200 border border-transparent hover:border-theme"
          >
            <div className="flex items-center gap-2 sm:gap-3">
              <div className="w-9 h-9 sm:w-10 sm:h-10 rounded-lg bg-theme-card flex items-center justify-center shadow-sm border border-theme-light">
                <Wifi className="w-4 h-4 sm:w-5 sm:h-5 text-accent" />
              </div>
              <div className="text-left">
                <div className="font-medium text-theme text-sm">
                  Direct Connection
                </div>
                <div className="text-xs text-theme-muted">brewos.local</div>
              </div>
            </div>
            <ChevronRight className="w-5 h-5 text-theme-muted group-hover:text-accent group-hover:translate-x-1 transition-all duration-200" />
          </a>
        </div>
      </div>

      {/* Footer */}
      <p
        className={`text-center text-xs text-theme-muted mt-6 ${
          animated ? "opacity-0 login-footer" : ""
        }`}
      >
        By signing in, you agree to our{" "}
        <a
          href={LEGAL_LINKS.terms}
          target="_blank"
          rel="noopener noreferrer"
          className="underline hover:text-theme-secondary transition-colors"
        >
          Terms of Service
        </a>{" "}
        and{" "}
        <a
          href={LEGAL_LINKS.privacy}
          target="_blank"
          rel="noopener noreferrer"
          className="underline hover:text-theme-secondary transition-colors"
        >
          Privacy Policy
        </a>
      </p>
    </div>
  );
}
