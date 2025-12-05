import { Card } from "@/components/Card";
import { darkBgStyles } from "@/lib/darkBgStyles";
import type { ReactNode } from "react";

interface OnboardingLayoutProps {
  children: ReactNode;
  /** Background gradient classes */
  gradient?: string;
  /** Max width for desktop card */
  maxWidth?: string;
  /** Desktop top padding */
  desktopTopPadding?: string;
}

/**
 * Shared layout component for onboarding/setup pages.
 * Handles responsive mobile (full-screen) and desktop (card) layouts.
 * Uses the `xs` breakpoint (400px) for switching between layouts.
 * Used by both production pages and Storybook stories to avoid duplication.
 */
export function OnboardingLayout({
  children,
  gradient = "bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950",
  maxWidth = "max-w-lg",
  desktopTopPadding = "pt-8",
}: OnboardingLayoutProps) {
  return (
    <div className="min-h-screen min-h-[100dvh]">
      {/* Narrow width: Full-screen scrollable with dark gradient */}
      <div
        className={`xs:hidden ${gradient} min-h-screen min-h-[100dvh] flex flex-col px-4 py-3 safe-area-inset`}
        style={darkBgStyles}
      >
        <div className="flex-1 flex flex-col justify-center overflow-y-auto animate-in fade-in slide-in-from-bottom-4 duration-500">
          <div className="py-2">{children}</div>
        </div>
      </div>

      {/* Wide width: Card layout with theme background */}
      <div
        className={`hidden xs:flex bg-theme min-h-screen justify-center items-start p-4 ${desktopTopPadding}`}
      >
        <div className={`w-full ${maxWidth}`}>
          <Card className="animate-in fade-in slide-in-from-bottom-4 duration-300">
            {children}
          </Card>
        </div>
      </div>
    </div>
  );
}
