import { ReactNode } from 'react';
import { cn } from '@/lib/utils';

interface CardProps {
  children: ReactNode;
  className?: string;
}

export function Card({ children, className }: CardProps) {
  return (
    <div className={cn('card', className)}>
      {children}
    </div>
  );
}

interface CardHeaderProps {
  children: ReactNode;
  className?: string;
  action?: ReactNode;
}

export function CardHeader({ children, className, action }: CardHeaderProps) {
  return (
    <div className={cn('flex items-center justify-between gap-4 mb-4', className)}>
      <div className="flex items-center gap-3">
        {children}
      </div>
      {action && <div>{action}</div>}
    </div>
  );
}

interface CardTitleProps {
  children: ReactNode;
  icon?: ReactNode;
}

export function CardTitle({ children, icon }: CardTitleProps) {
  return (
    <div className="flex items-center gap-3">
      {icon && <span className="text-accent">{icon}</span>}
      <h3 className="text-lg font-bold text-theme">{children}</h3>
    </div>
  );
}

interface CardDescriptionProps {
  children: ReactNode;
}

export function CardDescription({ children }: CardDescriptionProps) {
  return (
    <p className="text-sm text-coffee-500 mt-1">{children}</p>
  );
}
