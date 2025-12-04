import { cn } from '@/lib/utils';
import { useThemeStore } from '@/lib/themeStore';

interface LogoProps {
  size?: 'sm' | 'md' | 'lg' | 'xl';
  /** Show only the icon (no text) */
  iconOnly?: boolean;
  className?: string;
  /** Force light/white text (for dark backgrounds regardless of theme) */
  forceLight?: boolean;
  /** Force dark text (for light backgrounds regardless of theme) */
  forceDark?: boolean;
}

const sizes = {
  sm: { icon: 'h-7 w-7', text: 'h-3.5', gap: 'gap-1.5' },
  md: { icon: 'h-11 w-11', text: 'h-5', gap: 'gap-2' },
  lg: { icon: 'h-14 w-14', text: 'h-7', gap: 'gap-2.5' },
  xl: { icon: 'h-20 w-20', text: 'h-10', gap: 'gap-3' },
};

// Inline SVG text component for theme-aware coloring
function LogoText({ className, fill }: { className?: string; fill: string }) {
  return (
    <svg 
      xmlns="http://www.w3.org/2000/svg" 
      viewBox="100 800 1800 400"
      className={className}
      aria-label="brewos"
    >
      <path fill={fill} d="M292.22,905.54c-24.96-3.5-56.42-5.49-76.39,10l-4.01-78.38s-1.48-27.96-34.93-27.96-49.92,3.47-56.42,14.96v298.56s16.98,62.89,139.28,65.38c84.37,0,132.81-46.42,140.29-125.3,6.5-73.9-21.46-145.27-107.82-157.26ZM267.91,1115.89c-52.92,5.99-50.42-19.47-50.42-19.47v-102c0-7.65,15.49-20.81,40.45-19.81,44.43,1.99,44.43,54.4,44.43,54.4,0,0,10.48,80.87-34.46,86.87Z"/>
      <path fill={fill} d="M436.49,953.47v207.68s6.99,19.97,24.46,20.47c17.47.5,50.92,0,50.92,0,0,0,16.97,3.49,16.97-17.47v-169.24s22.47-24.46,69.89-19.97c18.47,1,20.47-6.49,22.47-21.47,2-14.98,8.49-48.93-44.93-48.43-53.42.5-99.76,1.13-139.79,48.43Z"/>
      <path fill={fill} d="M844.39,1110.23c-23.47,13.98-100.85,27.96-125.83-30.95,0,0,127.32-11.49,141.8-14.96,14.48-3.5,48.43-27.48,26.95-88.88-21.46-61.4-86.36-80.37-151.27-70.4-51.91,8.49-105.33,55.44-106.34,142.31-.98,86.87,51.43,131.3,134.29,142.28,62.41,6.98,119.33-17.98,120.32-49.92,1.01-31.96-16.47-43.45-39.92-29.47ZM755.5,972.43c31.46-4.99,46.45,9.5,56.92,37.45l-96.87,7.48s3-39.92,39.95-44.93Z"/>
      <path fill={fill} d="M908.94,930.84l57.91,226.32s7.99,31.29,65.23,27.96c39.94-3.33,47.93-6.66,57.91-51.26,9.98-44.6,22.63-104.51,22.63-104.51l30.62,119.15s9.32,36.44,63.24,36.86c38.61.42,57.91-10.23,63.9-37.52,5.99-27.29,46.6-204.36,46.6-204.36,0,0-4.66-38.29-42.6-38.45-37.94-.16-45.26,15.15-49.92,51.09-4.66,35.95-23.3,143.12-23.3,143.12l-29.29-136.46s-8.65-42.6-53.92-42.6-55.25,25.96-63.9,61.24c-8.65,35.28-26.63,116.49-26.63,116.49l-21.97-145.11s-2-49.6-47.93-47.77c-30.62,3.17-43.27,3.83-48.59,25.8Z"/>
      <path fill={fill} d="M1756.98,904.99s-95.19-4.11-111.16,81.1c-1.33,39.27,17.31,70.56,83.21,85.87,63.9,15.31,42.6,12.65,42.6,12.65,0,0,36.61,15.98-.67,33.28-23.3,9.98-67.9-2.66-84.54-11.32-16.64-8.65-22.63-7.32-28.62-.67-5.99,6.66-38.61,72.41,61.91,79.47,91.19,7.06,145.11-13.57,157.09-66.82,11.98-53.25-15.98-78.55-75.88-101.18-58.58-13.31-67.23-17.31-61.24-32.62,5.99-15.31,43.27-16.64,62.57-12.65,19.3,3.99,44.6,23.3,55.92,2.66,11.32-20.64,21.3-74.33-101.18-69.78Z"/>
      <path fill={fill} d="M1476.47,904.73s-144.83-3.17-140.18,152.59c3.98,120.48,107.18,133.68,153.09,129.42,45.94-4.29,127.15-24.26,133.15-152.05-2.66-72.55-61.82-134.13-146.06-129.95ZM1476.75,1116.56c-29.97-.67-39.27-19.97-47.26-64.57-7.34-71.88,47.26-77.23,47.26-77.23,36.61,2.69,45.27,23.31,47.26,75.89,1.99,52.61-24.62,66.59-47.26,65.91Z"/>
    </svg>
  );
}

export function Logo({ 
  size = 'md', 
  iconOnly = false,
  className,
  forceLight = false,
  forceDark = false,
}: LogoProps) {
  const { theme } = useThemeStore();
  const sizeConfig = sizes[size];
  
  // Determine text fill color based on theme or forced mode
  let textFill: string;
  
  if (forceLight) {
    textFill = '#ffffff';
  } else if (forceDark) {
    textFill = '#361e12'; // Original brand dark brown
  } else {
    // Use the theme's text color - already light for dark themes, dark for light themes
    textFill = theme.colors.text;
  }

  if (iconOnly) {
    return (
      <img 
        src="/logo-icon.svg"
        alt="BrewOS" 
        className={cn(sizeConfig.icon, className)}
      />
    );
  }

  return (
    <div className={cn('flex items-center', sizeConfig.gap, className)}>
      {/* Colorful icon - always full color */}
      <img 
        src="/logo-icon.svg"
        alt="" 
        className={cn(sizeConfig.icon, 'flex-shrink-0')}
      />
      {/* Text - color changes with theme */}
      <LogoText 
        className={cn(sizeConfig.text, 'w-auto')}
        fill={textFill}
      />
    </div>
  );
}

/** 
 * Standalone logo icon without text (always colorful)
 */
export function LogoIcon({ size = 'md', className }: { size?: 'sm' | 'md' | 'lg' | 'xl'; className?: string }) {
  const sizeConfig = sizes[size];
  
  return (
    <img 
      src="/logo-icon.svg"
      alt="BrewOS" 
      className={cn(sizeConfig.icon, className)}
    />
  );
}
