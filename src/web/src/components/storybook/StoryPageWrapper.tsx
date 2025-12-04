import { Card } from "@/components/Card";
import type { ReactNode } from "react";

interface StoryPageWrapperProps {
  children: ReactNode;
  className?: string;
  cardClassName?: string;
}

export function StoryPageWrapper({
  children,
  className,
  cardClassName,
}: StoryPageWrapperProps) {
  return (
    <div className={`min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex items-center justify-center p-4 ${className || ""}`}>
      <Card className={cardClassName}>{children}</Card>
    </div>
  );
}

