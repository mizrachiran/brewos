import { Card } from "@/components/Card";
import { darkBgStyles } from "@/lib/darkBgStyles";
import type { ReactNode } from "react";
import { useState, useEffect } from "react";

interface OnboardingLayoutProps {
  children: ReactNode;
  /** Background gradient classes */
  gradient?: string;
  /** Max width for desktop card */
  maxWidth?: string;
}

/**
 * Shared layout component for onboarding/setup pages.
 * Handles responsive mobile (full-screen) and desktop (card) layouts.
 * Uses the `xs` breakpoint (390px) for switching between layouts.
 * Also handles mobile landscape orientation specially.
 * Used by both production pages and Storybook stories to avoid duplication.
 *
 * NOTE: Uses JS-based responsive detection to ensure children only render once,
 * preventing issues with components that have side effects (like camera access).
 */
export function OnboardingLayout({
  children,
  gradient = "bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950",
  maxWidth = "max-w-lg",
}: OnboardingLayoutProps) {
  // Use JS-based responsive detection to render children only once
  // This prevents components with side effects (camera, etc.) from mounting twice
  const [isWideScreen, setIsWideScreen] = useState(() =>
    typeof window !== "undefined" ? window.innerWidth >= 390 : false
  );

  // Detect mobile landscape: landscape orientation with limited height
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const widthQuery = window.matchMedia("(min-width: 390px)");
    const landscapeQuery = window.matchMedia(
      "(orientation: landscape) and (max-height: 500px)"
    );

    const handleWidthChange = (e: MediaQueryListEvent | MediaQueryList) => {
      setIsWideScreen(e.matches);
    };

    const handleLandscapeChange = (e: MediaQueryListEvent | MediaQueryList) => {
      setIsMobileLandscape(e.matches);
    };

    // Set initial values
    handleWidthChange(widthQuery);
    handleLandscapeChange(landscapeQuery);

    // Listen for changes
    widthQuery.addEventListener("change", handleWidthChange);
    landscapeQuery.addEventListener("change", handleLandscapeChange);
    return () => {
      widthQuery.removeEventListener("change", handleWidthChange);
      landscapeQuery.removeEventListener("change", handleLandscapeChange);
    };
  }, []);

  // Mobile landscape: Card fills screen with margins
  if (isMobileLandscape) {
    return (
      <div className="h-[100dvh] overflow-hidden bg-theme flex items-center justify-center p-3">
        <Card className="w-full h-full flex flex-col animate-in fade-in slide-in-from-bottom-4 duration-300 overflow-hidden">
          <div className="flex-1 flex items-center justify-center">
            {children}
          </div>
        </Card>
      </div>
    );
  }

  if (isWideScreen) {
    // Wide width: Card layout centered on theme background
    return (
      <div className="min-h-[100dvh] overflow-y-auto">
        <div className="flex bg-theme min-h-[100dvh] justify-center items-center p-4">
          <div className={`w-full ${maxWidth}`}>
            <Card className="animate-in fade-in slide-in-from-bottom-4 duration-300">
              {children}
            </Card>
          </div>
        </div>
      </div>
    );
  }

  // Narrow width: Full-screen scrollable with dark gradient
  return (
    <div className="min-h-[100dvh]">
      <div
        className={`${gradient} min-h-[100dvh] flex flex-col px-4 pt-2 pb-4 safe-area-inset`}
        style={darkBgStyles}
      >
        <div className="flex-1 flex flex-col overflow-y-auto animate-in fade-in slide-in-from-bottom-4 duration-500">
          {children}
        </div>
      </div>
    </div>
  );
}
