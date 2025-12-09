import { useEffect, useState } from "react";
import { useNavigate, useLocation } from "react-router-dom";
import { GoogleLogin, CredentialResponse } from "@react-oauth/google";
import { LogoIcon } from "@/components/Logo";
import { useAuth } from "@/lib/auth";
import { isGoogleAuthConfigured } from "@/lib/auth";
import { useThemeStore } from "@/lib/themeStore";
import { ChevronRight } from "lucide-react";
import { LoginHero, LoginForm } from "@/components/login";

export function Login() {
  const navigate = useNavigate();
  const location = useLocation();
  const { user, handleGoogleLogin } = useAuth();
  const { theme } = useThemeStore();
  const isDark = theme.isDark;

  const error = (location.state as { error?: string })?.error;
  
  // Detect mobile landscape for optimized layout
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });
  
  useEffect(() => {
    const landscapeQuery = window.matchMedia("(orientation: landscape) and (max-height: 500px)");
    
    const handleLandscapeChange = (e: MediaQueryListEvent | MediaQueryList) => {
      setIsMobileLandscape(e.matches);
    };
    
    handleLandscapeChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleLandscapeChange);
    return () => landscapeQuery.removeEventListener("change", handleLandscapeChange);
  }, []);

  useEffect(() => {
    if (user) {
      navigate("/machines");
    }
  }, [user, navigate]);

  if (!isGoogleAuthConfigured) {
    return (
      <div className={`bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4 ${isMobileLandscape ? 'min-h-screen min-h-[100dvh] overflow-y-auto' : 'full-page'}`}>
        <div className={`w-full max-w-md text-center rounded-3xl bg-white/5 backdrop-blur-xl border border-white/10 login-card-enter ${isMobileLandscape ? 'p-4' : 'p-8'}`}>
          <div className={`flex justify-center ${isMobileLandscape ? 'mb-3' : 'mb-6'}`}>
            <LogoIcon size={isMobileLandscape ? "lg" : "xl"} />
          </div>
          <h1 className={`font-bold text-white ${isMobileLandscape ? 'text-xl mb-2' : 'text-2xl mb-3'}`}>
            Connect Locally
          </h1>
          <p className={`text-white/60 ${isMobileLandscape ? 'mb-3 text-sm' : 'mb-6'}`}>
            Cloud features are not configured. Connect directly to your device.
          </p>
          <a
            href="http://brewos.local"
            className={`inline-flex items-center gap-2 rounded-xl bg-accent text-white font-semibold hover:bg-accent-light transition-all duration-300 shadow-lg shadow-accent/30 ${isMobileLandscape ? 'px-4 py-2 text-sm' : 'px-6 py-3'}`}
          >
            Go to brewos.local
            <ChevronRight className="w-4 h-4" />
          </a>
        </div>
      </div>
    );
  }

  return (
    <div className={`flex flex-col lg:flex-row ${isMobileLandscape ? 'min-h-screen min-h-[100dvh] overflow-y-auto' : 'min-h-screen min-h-[100dvh]'}`}>
      {/* Left Panel - Hero/Brand Section (hidden on mobile and mobile landscape) */}
      {!isMobileLandscape && <LoginHero animated />}

      {/* Right Panel - Login Form */}
      <div className={`flex-1 lg:w-1/2 xl:w-[45%] flex items-center justify-center bg-theme relative ${isMobileLandscape ? 'p-3 overflow-y-auto' : 'p-4 sm:p-6 lg:p-8 overflow-y-auto min-h-screen min-h-[100dvh] lg:min-h-0'}`}>
        {/* Subtle decorative elements - hidden in mobile landscape */}
        {!isMobileLandscape && (
          <>
            <div className="absolute top-0 right-0 w-64 h-64 bg-gradient-to-bl from-accent/5 to-transparent rounded-full blur-3xl" />
            <div className="absolute bottom-0 left-0 w-96 h-96 bg-gradient-to-tr from-accent/5 to-transparent rounded-full blur-3xl" />
          </>
        )}

        <LoginForm
          error={error}
          animated
          compact={isMobileLandscape}
          googleButton={
            <GoogleLogin
              onSuccess={(response: CredentialResponse) => {
                if (response.credential) {
                  handleGoogleLogin(response.credential);
                }
              }}
              onError={() => {
                console.error("Google login failed");
              }}
              theme={isDark ? "filled_black" : "outline"}
              size="large"
              width="280"
              text="continue_with"
              shape="pill"
            />
          }
        />
      </div>
    </div>
  );
}
