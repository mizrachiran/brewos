import React from 'react';
import { useThemeStore } from '@/lib/themeStore';
import { themeList } from '@/lib/themes';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { cn } from '@/lib/utils';
import { Palette, Sun, Moon, Check } from 'lucide-react';

export function ThemeSettings() {
  const { theme: currentTheme, setTheme } = useThemeStore();

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Palette className="w-5 h-5" />}>
          Theme
        </CardTitle>
      </CardHeader>
      
      {/* Current Theme Badge */}
      <div className="flex items-center gap-3 mb-6 p-3 bg-theme-secondary rounded-xl">
        <div 
          className="w-10 h-10 rounded-lg shadow-sm flex items-center justify-center flex-shrink-0"
          style={{ backgroundColor: currentTheme.preview.primary }}
        >
          {currentTheme.isDark ? (
            <Moon className="w-5 h-5" style={{ color: currentTheme.preview.secondary }} />
          ) : (
            <Sun className="w-5 h-5" style={{ color: currentTheme.preview.secondary }} />
          )}
        </div>
        <div>
          <p className="font-semibold text-theme">Current: {currentTheme.name}</p>
          <p className="text-xs text-theme-muted">{currentTheme.description}</p>
        </div>
      </div>

      {/* Light Themes Section */}
      <div className="mb-6">
        <div className="flex items-center gap-2 mb-3">
          <Sun className="w-4 h-4 text-amber-500" />
          <h4 className="text-sm font-semibold text-theme-secondary uppercase tracking-wider">Light</h4>
        </div>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
          {themeList.filter(t => !t.isDark).map((theme) => {
            const isActive = currentTheme.id === theme.id;
            return (
              <button
                key={theme.id}
                onClick={() => setTheme(theme.id)}
                className={cn(
                  'relative p-3 rounded-xl border-2 transition-all text-left group overflow-hidden',
                  isActive
                    ? 'ring-2 ring-offset-2'
                    : 'hover:scale-[1.02]'
                )}
                style={{
                  backgroundColor: theme.colors.bg,
                  borderColor: isActive ? theme.preview.accent : theme.colors.border,
                  '--tw-ring-color': theme.preview.accent,
                  '--tw-ring-offset-color': 'var(--bg)',
                } as React.CSSProperties}
              >
                {/* Theme Preview Colors */}
                <div className="flex gap-1.5 mb-2">
                  <div 
                    className="w-6 h-6 rounded-md shadow-sm ring-1 ring-black/10"
                    style={{ backgroundColor: theme.colors.cardBg }}
                    title="Card"
                  />
                  <div 
                    className="w-6 h-6 rounded-md shadow-sm ring-1 ring-black/10"
                    style={{ backgroundColor: theme.preview.primary }}
                    title="Primary"
                  />
                  <div 
                    className="w-6 h-6 rounded-md shadow-sm ring-1 ring-black/10"
                    style={{ backgroundColor: theme.preview.accent }}
                    title="Accent"
                  />
                </div>
                
                {/* Theme Info */}
                <div className="flex items-center gap-2">
                  <span className="text-sm font-medium" style={{ color: theme.colors.text }}>{theme.name}</span>
                  {isActive && (
                    <Check className="w-3.5 h-3.5" style={{ color: theme.preview.accent }} />
                  )}
                </div>
              </button>
            );
          })}
        </div>
      </div>

      {/* Dark Themes Section */}
      <div>
        <div className="flex items-center gap-2 mb-3">
          <Moon className="w-4 h-4 text-indigo-400" />
          <h4 className="text-sm font-semibold text-theme-secondary uppercase tracking-wider">Dark</h4>
        </div>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
          {themeList.filter(t => t.isDark).map((theme) => {
            const isActive = currentTheme.id === theme.id;
            return (
              <button
                key={theme.id}
                onClick={() => setTheme(theme.id)}
                className={cn(
                  'relative p-3 rounded-xl border-2 transition-all text-left group overflow-hidden',
                  isActive
                    ? 'ring-2 ring-offset-2'
                    : 'hover:scale-[1.02]'
                )}
                style={{
                  backgroundColor: theme.colors.bg,
                  borderColor: isActive ? theme.preview.accent : theme.colors.border,
                  '--tw-ring-color': theme.preview.accent,
                  '--tw-ring-offset-color': 'var(--bg)',
                } as React.CSSProperties}
              >
                {/* Theme Preview Colors */}
                <div className="flex gap-1.5 mb-2">
                  <div 
                    className="w-6 h-6 rounded-md shadow-sm ring-1 ring-white/20"
                    style={{ backgroundColor: theme.colors.cardBg }}
                    title="Card"
                  />
                  <div 
                    className="w-6 h-6 rounded-md shadow-sm ring-1 ring-white/20"
                    style={{ backgroundColor: theme.preview.primary }}
                    title="Primary"
                  />
                  <div 
                    className="w-6 h-6 rounded-md shadow-sm ring-1 ring-white/20"
                    style={{ backgroundColor: theme.preview.accent }}
                    title="Accent"
                  />
                </div>
                
                {/* Theme Info */}
                <div className="flex items-center gap-2">
                  <span className="text-sm font-medium" style={{ color: theme.colors.text }}>{theme.name}</span>
                  {isActive && (
                    <Check className="w-3.5 h-3.5" style={{ color: theme.preview.accent }} />
                  )}
                </div>
              </button>
            );
          })}
        </div>
      </div>
    </Card>
  );
}

