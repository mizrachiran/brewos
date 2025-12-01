// Theme definitions for BrewOS
// Each theme has a warm, coffee-inspired palette with distinct visual identity

export type ThemeId = 
  | 'classic' 
  | 'mocha' 
  | 'espresso' 
  | 'latte' 
  | 'caramel'
  | 'warm-mocha' 
  | 'roasted'
  | 'dark-roast'
  | 'midnight';

export interface ThemeColors {
  // Base backgrounds
  bg: string;
  bgSecondary: string;
  bgTertiary: string;
  
  // Text colors
  text: string;
  textSecondary: string;
  textMuted: string;
  
  // Coffee palette (primary)
  coffee50: string;
  coffee100: string;
  coffee200: string;
  coffee300: string;
  coffee400: string;
  coffee500: string;
  coffee600: string;
  coffee700: string;
  coffee800: string;
  coffee900: string;
  
  // Cream palette (secondary)
  cream100: string;
  cream200: string;
  cream300: string;
  cream400: string;
  
  // Accent color
  accent: string;
  accentLight: string;
  accentGlow: string;
  
  // Card & borders
  cardBg: string;
  border: string;
  borderLight: string;
  
  // Shadows
  shadowColor: string;
  
  // Navigation
  navBg: string;
  navActiveBg: string;
  navActiveText: string;
  navInactiveText: string;
  navHoverBg: string;
  
  // Header
  headerGlassBg: string;
  headerGlassBlur: string;
}

export interface Theme {
  id: ThemeId;
  name: string;
  description: string;
  preview: {
    primary: string;
    secondary: string;
    accent: string;
  };
  isDark: boolean;
  colors: ThemeColors;
}

export const themes: Record<ThemeId, Theme> = {
  classic: {
    id: 'classic',
    name: 'Classic Coffee',
    description: 'Warm and inviting browns',
    preview: {
      primary: '#391f12',
      secondary: '#faf8f5',
      accent: '#c4703c',
    },
    isDark: false,
    colors: {
      bg: '#faf8f5',
      bgSecondary: '#f5f0e8',
      bgTertiary: '#e8e0d5',
      text: '#1a0f0a',
      textSecondary: '#5c3d2e',
      textMuted: '#9b7a68',
      coffee50: '#faf8f5',
      coffee100: '#f5f0e8',
      coffee200: '#e8e0d5',
      coffee300: '#d4c8b8',
      coffee400: '#b5a08a',
      coffee500: '#9b7a68',
      coffee600: '#7c5a47',
      coffee700: '#5c3d2e',
      coffee800: '#391f12',
      coffee900: '#1a0f0a',
      cream100: '#faf8f5',
      cream200: '#f5f0e8',
      cream300: '#e8e0d5',
      cream400: '#d4c8b8',
      accent: '#c4703c',
      accentLight: '#e8a066',
      accentGlow: 'rgba(196, 112, 60, 0.25)',
      cardBg: '#ffffff',
      border: '#e8e0d5',
      borderLight: '#f5f0e8',
      shadowColor: 'rgba(26, 15, 10, 0.08)',
      navBg: '#ffffff',
      navActiveBg: '#391f12',
      navActiveText: '#ffffff',
      navInactiveText: '#7c5a47',
      navHoverBg: '#f5f0e8',
      headerGlassBg: 'rgba(255, 255, 255, 0.85)',
      headerGlassBlur: '12px',
    },
  },
  
  mocha: {
    id: 'mocha',
    name: 'Mocha',
    description: 'Rich chocolate with purple hints',
    preview: {
      primary: '#3d1f2e',
      secondary: '#f5f0f2',
      accent: '#9c5a7c',
    },
    isDark: false,
    colors: {
      bg: '#f7f2f4',
      bgSecondary: '#efe6ea',
      bgTertiary: '#e2d5dc',
      text: '#2d1520',
      textSecondary: '#5a3545',
      textMuted: '#8a6878',
      coffee50: '#f7f2f4',
      coffee100: '#efe6ea',
      coffee200: '#e2d5dc',
      coffee300: '#cbb8c2',
      coffee400: '#a88898',
      coffee500: '#8a6878',
      coffee600: '#6a4858',
      coffee700: '#4d3040',
      coffee800: '#3d1f2e',
      coffee900: '#2d1520',
      cream100: '#f7f2f4',
      cream200: '#efe6ea',
      cream300: '#e2d5dc',
      cream400: '#cbb8c2',
      accent: '#9c5a7c',
      accentLight: '#c48aa8',
      accentGlow: 'rgba(156, 90, 124, 0.25)',
      cardBg: '#ffffff',
      border: '#e2d5dc',
      borderLight: '#efe6ea',
      shadowColor: 'rgba(45, 21, 32, 0.1)',
      navBg: '#ffffff',
      navActiveBg: '#3d1f2e',
      navActiveText: '#ffffff',
      navInactiveText: '#6a4858',
      navHoverBg: '#efe6ea',
      headerGlassBg: 'rgba(255, 255, 255, 0.88)',
      headerGlassBlur: '12px',
    },
  },
  
  espresso: {
    id: 'espresso',
    name: 'Espresso',
    description: 'Deep and bold with golden accent',
    preview: {
      primary: '#1a1612',
      secondary: '#f5f2ef',
      accent: '#d4a45c',
    },
    isDark: false,
    colors: {
      bg: '#f5f2ef',
      bgSecondary: '#ebe6e0',
      bgTertiary: '#dcd5cc',
      text: '#1a1612',
      textSecondary: '#3d3530',
      textMuted: '#6b5f55',
      coffee50: '#f5f2ef',
      coffee100: '#ebe6e0',
      coffee200: '#dcd5cc',
      coffee300: '#c4b8aa',
      coffee400: '#9a8a7a',
      coffee500: '#6b5f55',
      coffee600: '#4d433a',
      coffee700: '#332b25',
      coffee800: '#241c18',
      coffee900: '#1a1612',
      cream100: '#f5f2ef',
      cream200: '#ebe6e0',
      cream300: '#dcd5cc',
      cream400: '#c4b8aa',
      accent: '#d4a45c',
      accentLight: '#e8c48a',
      accentGlow: 'rgba(212, 164, 92, 0.3)',
      cardBg: '#ffffff',
      border: '#dcd5cc',
      borderLight: '#ebe6e0',
      shadowColor: 'rgba(26, 22, 18, 0.1)',
      navBg: '#ffffff',
      navActiveBg: '#1a1612',
      navActiveText: '#d4a45c',
      navInactiveText: '#6b5f55',
      navHoverBg: '#ebe6e0',
      headerGlassBg: 'rgba(255, 255, 255, 0.9)',
      headerGlassBlur: '14px',
    },
  },
  
  latte: {
    id: 'latte',
    name: 'Latte',
    description: 'Light, airy, and creamy',
    preview: {
      primary: '#6b5a4d',
      secondary: '#fdfbf8',
      accent: '#7ca67c',
    },
    isDark: false,
    colors: {
      bg: '#fdfbf8',
      bgSecondary: '#f8f4ef',
      bgTertiary: '#f0ebe4',
      text: '#3d352e',
      textSecondary: '#6b5a4d',
      textMuted: '#9a8a78',
      coffee50: '#fdfbf8',
      coffee100: '#f8f4ef',
      coffee200: '#f0ebe4',
      coffee300: '#e0d8ce',
      coffee400: '#c8b8a5',
      coffee500: '#a69280',
      coffee600: '#7a6858',
      coffee700: '#6b5a4d',
      coffee800: '#4d4238',
      coffee900: '#3d352e',
      cream100: '#fdfbf8',
      cream200: '#f8f4ef',
      cream300: '#f0ebe4',
      cream400: '#e0d8ce',
      accent: '#7ca67c',
      accentLight: '#a8c8a8',
      accentGlow: 'rgba(124, 166, 124, 0.25)',
      cardBg: '#ffffff',
      border: '#f0ebe4',
      borderLight: '#f8f4ef',
      shadowColor: 'rgba(61, 53, 46, 0.06)',
      navBg: '#ffffff',
      navActiveBg: '#6b5a4d',
      navActiveText: '#ffffff',
      navInactiveText: '#9a8a78',
      navHoverBg: '#f8f4ef',
      headerGlassBg: 'rgba(255, 255, 255, 0.92)',
      headerGlassBlur: '10px',
    },
  },
  
  caramel: {
    id: 'caramel',
    name: 'Caramel',
    description: 'Warm amber and honey tones',
    preview: {
      primary: '#5a3818',
      secondary: '#fdf6ec',
      accent: '#e8853c',
    },
    isDark: false,
    colors: {
      bg: '#fdf6ec',
      bgSecondary: '#f8eedc',
      bgTertiary: '#f0e2c8',
      text: '#3a2510',
      textSecondary: '#6a4520',
      textMuted: '#a87840',
      coffee50: '#fdf6ec',
      coffee100: '#f8eedc',
      coffee200: '#f0e2c8',
      coffee300: '#e0cca8',
      coffee400: '#c8a878',
      coffee500: '#a87840',
      coffee600: '#885830',
      coffee700: '#6a4520',
      coffee800: '#5a3818',
      coffee900: '#3a2510',
      cream100: '#fdf6ec',
      cream200: '#f8eedc',
      cream300: '#f0e2c8',
      cream400: '#e0cca8',
      accent: '#e8853c',
      accentLight: '#f0a868',
      accentGlow: 'rgba(232, 133, 60, 0.35)',
      cardBg: '#fffefa',
      border: '#f0e2c8',
      borderLight: '#f8eedc',
      shadowColor: 'rgba(58, 37, 16, 0.1)',
      navBg: '#fffefa',
      navActiveBg: '#5a3818',
      navActiveText: '#f8eedc',
      navInactiveText: '#885830',
      navHoverBg: '#f8eedc',
      headerGlassBg: 'rgba(253, 246, 236, 0.9)',
      headerGlassBlur: '12px',
    },
  },
  
  // NEW: Warm Mocha - light mocha background, white cards, warm text
  'warm-mocha': {
    id: 'warm-mocha',
    name: 'Warm Mocha',
    description: 'Soft mocha with warm accents',
    preview: {
      primary: '#4a3228',
      secondary: '#f5ebe5',
      accent: '#c67c4e',
    },
    isDark: false,
    colors: {
      bg: '#f5ebe5',
      bgSecondary: '#ede0d8',
      bgTertiary: '#e0d0c5',
      text: '#2d1f18',
      textSecondary: '#5a4035',
      textMuted: '#8a7065',
      coffee50: '#f5ebe5',
      coffee100: '#ede0d8',
      coffee200: '#e0d0c5',
      coffee300: '#cbb8aa',
      coffee400: '#a89080',
      coffee500: '#8a7065',
      coffee600: '#6a5048',
      coffee700: '#4a3228',
      coffee800: '#3a251c',
      coffee900: '#2d1f18',
      cream100: '#f5ebe5',
      cream200: '#ede0d8',
      cream300: '#e0d0c5',
      cream400: '#cbb8aa',
      accent: '#c67c4e',
      accentLight: '#e0a070',
      accentGlow: 'rgba(198, 124, 78, 0.3)',
      cardBg: '#ffffff',
      border: '#e0d0c5',
      borderLight: '#ede0d8',
      shadowColor: 'rgba(45, 31, 24, 0.1)',
      navBg: '#ffffff',
      navActiveBg: '#4a3228',
      navActiveText: '#ffffff',
      navInactiveText: '#6a5048',
      navHoverBg: '#ede0d8',
      headerGlassBg: 'rgba(255, 255, 255, 0.88)',
      headerGlassBlur: '12px',
    },
  },
  
  // FIXED: Roasted is now a proper dark theme
  roasted: {
    id: 'roasted',
    name: 'Roasted',
    description: 'Rich dark coffee atmosphere',
    preview: {
      primary: '#e8d4c4',
      secondary: '#2a1810',
      accent: '#e89050',
    },
    isDark: true,
    colors: {
      bg: '#2a1810',
      bgSecondary: '#3d2820',
      bgTertiary: '#4a3428',
      text: '#f5ebe5',
      textSecondary: '#d4c0b0',
      textMuted: '#a08878',
      coffee50: '#f5ebe5',
      coffee100: '#e8d4c4',
      coffee200: '#d4b8a0',
      coffee300: '#a08878',
      coffee400: '#785848',
      coffee500: '#5a4038',
      coffee600: '#4a3428',
      coffee700: '#3d2820',
      coffee800: '#2a1810',
      coffee900: '#1a100a',
      cream100: '#3d2820',
      cream200: '#4a3428',
      cream300: '#5a4438',
      cream400: '#6a5448',
      accent: '#e89050',
      accentLight: '#f0b080',
      accentGlow: 'rgba(232, 144, 80, 0.4)',
      cardBg: '#3a2518',
      border: '#5a4038',
      borderLight: '#4a3428',
      shadowColor: 'rgba(0, 0, 0, 0.4)',
      navBg: '#2a1810',
      navActiveBg: '#e89050',
      navActiveText: '#1a100a',
      navInactiveText: '#a08878',
      navHoverBg: '#3d2820',
      headerGlassBg: 'rgba(42, 24, 16, 0.95)',
      headerGlassBlur: '12px',
    },
  },
  
  'dark-roast': {
    id: 'dark-roast',
    name: 'Dark Roast',
    description: 'Deep and intense dark mode',
    preview: {
      primary: '#f5dcc8',
      secondary: '#141210',
      accent: '#d4703c',
    },
    isDark: true,
    colors: {
      bg: '#0e0c0a',
      bgSecondary: '#181410',
      bgTertiary: '#241e18',
      text: '#f5e8d8',
      textSecondary: '#d4c4a8',
      textMuted: '#a89878',
      coffee50: '#f5e8d8',
      coffee100: '#d4c4a8',
      coffee200: '#b0a088',
      coffee300: '#a89878',
      coffee400: '#786850',
      coffee500: '#584838',
      coffee600: '#352818',
      coffee700: '#241e18',
      coffee800: '#181410',
      coffee900: '#0e0c0a',
      cream100: '#241e18',
      cream200: '#302820',
      cream300: '#403830',
      cream400: '#504840',
      accent: '#d4703c',
      accentLight: '#e89868',
      accentGlow: 'rgba(212, 112, 60, 0.4)',
      cardBg: '#1a1614',
      border: '#352818',
      borderLight: '#241e18',
      shadowColor: 'rgba(0, 0, 0, 0.5)',
      navBg: '#141210',
      navActiveBg: '#d4703c',
      navActiveText: '#0e0c0a',
      navInactiveText: '#a89878',
      navHoverBg: '#241e18',
      headerGlassBg: 'rgba(14, 12, 10, 0.9)',
      headerGlassBlur: '14px',
    },
  },
  
  midnight: {
    id: 'midnight',
    name: 'Midnight Brew',
    description: 'Elegant dark with teal accents',
    preview: {
      primary: '#e0d4c4',
      secondary: '#0c0e10',
      accent: '#4aa8a8',
    },
    isDark: true,
    colors: {
      bg: '#0a0c0e',
      bgSecondary: '#121618',
      bgTertiary: '#1a2024',
      text: '#e8e0d4',
      textSecondary: '#c8b8a0',
      textMuted: '#888078',
      coffee50: '#e8e0d4',
      coffee100: '#c8b8a0',
      coffee200: '#a89880',
      coffee300: '#888078',
      coffee400: '#585450',
      coffee500: '#383838',
      coffee600: '#222428',
      coffee700: '#1a2024',
      coffee800: '#121618',
      coffee900: '#0a0c0e',
      cream100: '#1a2024',
      cream200: '#222428',
      cream300: '#2a3038',
      cream400: '#384048',
      accent: '#4aa8a8',
      accentLight: '#78c8c8',
      accentGlow: 'rgba(74, 168, 168, 0.35)',
      cardBg: '#141820',
      border: '#222428',
      borderLight: '#1a2024',
      shadowColor: 'rgba(0, 0, 0, 0.6)',
      navBg: '#0e1214',
      navActiveBg: '#4aa8a8',
      navActiveText: '#0a0c0e',
      navInactiveText: '#888078',
      navHoverBg: '#1a2024',
      headerGlassBg: 'rgba(10, 12, 14, 0.92)',
      headerGlassBlur: '16px',
    },
  },
};

export const themeList = Object.values(themes);

// Apply theme to document
export function applyTheme(theme: Theme) {
  const root = document.documentElement;
  const { colors } = theme;
  
  // Set CSS custom properties
  root.style.setProperty('--bg', colors.bg);
  root.style.setProperty('--bg-secondary', colors.bgSecondary);
  root.style.setProperty('--bg-tertiary', colors.bgTertiary);
  root.style.setProperty('--text', colors.text);
  root.style.setProperty('--text-secondary', colors.textSecondary);
  root.style.setProperty('--text-muted', colors.textMuted);
  
  root.style.setProperty('--coffee-50', colors.coffee50);
  root.style.setProperty('--coffee-100', colors.coffee100);
  root.style.setProperty('--coffee-200', colors.coffee200);
  root.style.setProperty('--coffee-300', colors.coffee300);
  root.style.setProperty('--coffee-400', colors.coffee400);
  root.style.setProperty('--coffee-500', colors.coffee500);
  root.style.setProperty('--coffee-600', colors.coffee600);
  root.style.setProperty('--coffee-700', colors.coffee700);
  root.style.setProperty('--coffee-800', colors.coffee800);
  root.style.setProperty('--coffee-900', colors.coffee900);
  
  root.style.setProperty('--cream-100', colors.cream100);
  root.style.setProperty('--cream-200', colors.cream200);
  root.style.setProperty('--cream-300', colors.cream300);
  root.style.setProperty('--cream-400', colors.cream400);
  
  root.style.setProperty('--accent', colors.accent);
  root.style.setProperty('--accent-light', colors.accentLight);
  root.style.setProperty('--accent-glow', colors.accentGlow);
  
  root.style.setProperty('--card-bg', colors.cardBg);
  root.style.setProperty('--border', colors.border);
  root.style.setProperty('--border-light', colors.borderLight);
  root.style.setProperty('--shadow-color', colors.shadowColor);
  
  // Navigation
  root.style.setProperty('--nav-bg', colors.navBg);
  root.style.setProperty('--nav-active-bg', colors.navActiveBg);
  root.style.setProperty('--nav-active-text', colors.navActiveText);
  root.style.setProperty('--nav-inactive-text', colors.navInactiveText);
  root.style.setProperty('--nav-hover-bg', colors.navHoverBg);
  
  // Header
  root.style.setProperty('--header-glass-bg', colors.headerGlassBg);
  root.style.setProperty('--header-glass-blur', colors.headerGlassBlur);
  
  // Set dark mode class
  if (theme.isDark) {
    root.classList.add('dark');
  } else {
    root.classList.remove('dark');
  }
  
  // Update meta theme-color for mobile browsers
  const metaThemeColor = document.querySelector('meta[name="theme-color"]');
  if (metaThemeColor) {
    metaThemeColor.setAttribute('content', colors.bg);
  }
  
  // Update Apple status bar style based on theme
  const metaStatusBar = document.querySelector('meta[name="apple-mobile-web-app-status-bar-style"]');
  if (metaStatusBar) {
    // black-translucent = light content (for dark backgrounds)
    // default = dark content (for light backgrounds)
    metaStatusBar.setAttribute('content', theme.isDark ? 'black-translucent' : 'default');
  }
  
  // Store preference
  localStorage.setItem('brewos-theme', theme.id);
}

// Get stored theme or default
export function getStoredTheme(): Theme {
  const stored = localStorage.getItem('brewos-theme') as ThemeId | null;
  if (stored && themes[stored]) {
    return themes[stored];
  }
  return themes.classic;
}
