// Theme definitions for BrewOS
// Each theme has a warm, coffee-inspired palette with distinct visual identity

export type ThemeId =
  | "caramel"
  | "toffee"
  | "classic"
  | "mulberry"
  | "espresso"
  | "latte"
  | "rosetta"
  | "cappuccino"
  | "cortado"
  | "roasted"
  | "dark-roast"
  | "midnight";

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
  // Caramel - Vibrant honey gold with sunset orange accent
  caramel: {
    id: "caramel",
    name: "Caramel",
    description: "Golden honey with sunset warmth",
    preview: {
      primary: "#6d4c1a",
      secondary: "#fef7e8",
      accent: "#e07020",
    },
    isDark: false,
    colors: {
      bg: "#fef7e8",
      bgSecondary: "#fcefd5",
      bgTertiary: "#f8e4be",
      text: "#422a08",
      textSecondary: "#6d4c1a",
      textMuted: "#a07830",
      coffee50: "#fef7e8",
      coffee100: "#fcefd5",
      coffee200: "#f8e4be",
      coffee300: "#f0d49a",
      coffee400: "#d4a850",
      coffee500: "#a07830",
      coffee600: "#7a5820",
      coffee700: "#6d4c1a",
      coffee800: "#523810",
      coffee900: "#422a08",
      cream100: "#fef7e8",
      cream200: "#fcefd5",
      cream300: "#f8e4be",
      cream400: "#f0d49a",
      accent: "#e07020",
      accentLight: "#f09050",
      accentGlow: "rgba(224, 112, 32, 0.35)",
      cardBg: "#fffcf5",
      border: "#f8e4be",
      borderLight: "#fcefd5",
      shadowColor: "rgba(66, 42, 8, 0.12)",
      navBg: "#fffcf5",
      navActiveBg: "#e07020",
      navActiveText: "#ffffff",
      navInactiveText: "#7a5820",
      navHoverBg: "#fcefd5",
      headerGlassBg: "rgba(254, 247, 232, 0.92)",
      headerGlassBlur: "12px",
    },
  },

  // Toffee - Warm caramel-coffee browns with cinnamon accent
  toffee: {
    id: "toffee",
    name: "Toffee",
    description: "Warm caramel-coffee browns",
    preview: {
      primary: "#4a3020",
      secondary: "#f8f0e8",
      accent: "#c06030",
    },
    isDark: false,
    colors: {
      bg: "#f8f0e8",
      bgSecondary: "#f0e4d8",
      bgTertiary: "#e8d4c4",
      text: "#2e1c14",
      textSecondary: "#4a3020",
      textMuted: "#8a6850",
      coffee50: "#f8f0e8",
      coffee100: "#f0e4d8",
      coffee200: "#e8d4c4",
      coffee300: "#d8c0a8",
      coffee400: "#c0a080",
      coffee500: "#987050",
      coffee600: "#705038",
      coffee700: "#4a3020",
      coffee800: "#3a2418",
      coffee900: "#2e1c14",
      cream100: "#f8f0e8",
      cream200: "#f0e4d8",
      cream300: "#e8d4c4",
      cream400: "#d8c0a8",
      accent: "#c06030",
      accentLight: "#d88050",
      accentGlow: "rgba(192, 96, 48, 0.32)",
      cardBg: "#fffaf5",
      border: "#e8d4c4",
      borderLight: "#f0e4d8",
      shadowColor: "rgba(46, 28, 20, 0.12)",
      navBg: "#fffaf5",
      navActiveBg: "#4a3020",
      navActiveText: "#f0e4d8",
      navInactiveText: "#705038",
      navHoverBg: "#f0e4d8",
      headerGlassBg: "rgba(248, 240, 232, 0.92)",
      headerGlassBlur: "12px",
    },
  },

  // Classic - Clean sepia newsprint aesthetic with burgundy accent
  classic: {
    id: "classic",
    name: "Classic",
    description: "Timeless sepia with burgundy",
    preview: {
      primary: "#3c2415",
      secondary: "#f8f5f0",
      accent: "#8c3a3a",
    },
    isDark: false,
    colors: {
      bg: "#f8f5f0",
      bgSecondary: "#f0ebe2",
      bgTertiary: "#e5ddd0",
      text: "#2a1810",
      textSecondary: "#4a3525",
      textMuted: "#7a6555",
      coffee50: "#f8f5f0",
      coffee100: "#f0ebe2",
      coffee200: "#e5ddd0",
      coffee300: "#d5c8b5",
      coffee400: "#b5a590",
      coffee500: "#907860",
      coffee600: "#6a5545",
      coffee700: "#4a3525",
      coffee800: "#3c2415",
      coffee900: "#2a1810",
      cream100: "#f8f5f0",
      cream200: "#f0ebe2",
      cream300: "#e5ddd0",
      cream400: "#d5c8b5",
      accent: "#8c3a3a",
      accentLight: "#b86060",
      accentGlow: "rgba(140, 58, 58, 0.25)",
      cardBg: "#fffefa",
      border: "#e5ddd0",
      borderLight: "#f0ebe2",
      shadowColor: "rgba(42, 24, 16, 0.1)",
      navBg: "#fffefa",
      navActiveBg: "#8c3a3a",
      navActiveText: "#ffffff",
      navInactiveText: "#6a5545",
      navHoverBg: "#f0ebe2",
      headerGlassBg: "rgba(255, 254, 250, 0.9)",
      headerGlassBlur: "12px",
    },
  },

  // Mulberry - Berry wine with rose dust
  mulberry: {
    id: "mulberry",
    name: "Mulberry",
    description: "Berry wine with rose dust",
    preview: {
      primary: "#4a2038",
      secondary: "#faf4f6",
      accent: "#a04878",
    },
    isDark: false,
    colors: {
      bg: "#faf4f6",
      bgSecondary: "#f4e8ed",
      bgTertiary: "#eadae2",
      text: "#321828",
      textSecondary: "#5a3048",
      textMuted: "#906878",
      coffee50: "#faf4f6",
      coffee100: "#f4e8ed",
      coffee200: "#eadae2",
      coffee300: "#d8c0cc",
      coffee400: "#b898a8",
      coffee500: "#906878",
      coffee600: "#704858",
      coffee700: "#5a3048",
      coffee800: "#4a2038",
      coffee900: "#321828",
      cream100: "#faf4f6",
      cream200: "#f4e8ed",
      cream300: "#eadae2",
      cream400: "#d8c0cc",
      accent: "#a04878",
      accentLight: "#c878a0",
      accentGlow: "rgba(160, 72, 120, 0.28)",
      cardBg: "#ffffff",
      border: "#eadae2",
      borderLight: "#f4e8ed",
      shadowColor: "rgba(50, 24, 40, 0.1)",
      navBg: "#ffffff",
      navActiveBg: "#a04878",
      navActiveText: "#ffffff",
      navInactiveText: "#704858",
      navHoverBg: "#f4e8ed",
      headerGlassBg: "rgba(255, 255, 255, 0.9)",
      headerGlassBlur: "12px",
    },
  },

  // Espresso - Cool slate sophistication with brass accent
  espresso: {
    id: "espresso",
    name: "Espresso",
    description: "Cool slate with brass elegance",
    preview: {
      primary: "#1e1c1a",
      secondary: "#f4f3f1",
      accent: "#b8963c",
    },
    isDark: false,
    colors: {
      bg: "#f4f3f1",
      bgSecondary: "#eae8e4",
      bgTertiary: "#dedad4",
      text: "#1e1c1a",
      textSecondary: "#3a3836",
      textMuted: "#6a6662",
      coffee50: "#f4f3f1",
      coffee100: "#eae8e4",
      coffee200: "#dedad4",
      coffee300: "#c8c4bc",
      coffee400: "#a8a49c",
      coffee500: "#7a7670",
      coffee600: "#545250",
      coffee700: "#3a3836",
      coffee800: "#2a2826",
      coffee900: "#1e1c1a",
      cream100: "#f4f3f1",
      cream200: "#eae8e4",
      cream300: "#dedad4",
      cream400: "#c8c4bc",
      accent: "#b8963c",
      accentLight: "#d4b460",
      accentGlow: "rgba(184, 150, 60, 0.3)",
      cardBg: "#fafaf9",
      border: "#dedad4",
      borderLight: "#eae8e4",
      shadowColor: "rgba(30, 28, 26, 0.1)",
      navBg: "#fafaf9",
      navActiveBg: "#1e1c1a",
      navActiveText: "#b8963c",
      navInactiveText: "#6a6662",
      navHoverBg: "#eae8e4",
      headerGlassBg: "rgba(250, 250, 249, 0.92)",
      headerGlassBlur: "14px",
    },
  },

  // Latte - Fresh minimalist with sage green accent
  latte: {
    id: "latte",
    name: "Latte",
    description: "Fresh minimal with sage",
    preview: {
      primary: "#4a5548",
      secondary: "#fafcfa",
      accent: "#5a8a5a",
    },
    isDark: false,
    colors: {
      bg: "#fafcfa",
      bgSecondary: "#f2f6f2",
      bgTertiary: "#e8efe8",
      text: "#2a322a",
      textSecondary: "#4a5548",
      textMuted: "#788078",
      coffee50: "#fafcfa",
      coffee100: "#f2f6f2",
      coffee200: "#e8efe8",
      coffee300: "#d4ddd4",
      coffee400: "#b0bab0",
      coffee500: "#889088",
      coffee600: "#606860",
      coffee700: "#4a5548",
      coffee800: "#384038",
      coffee900: "#2a322a",
      cream100: "#fafcfa",
      cream200: "#f2f6f2",
      cream300: "#e8efe8",
      cream400: "#d4ddd4",
      accent: "#5a8a5a",
      accentLight: "#80b080",
      accentGlow: "rgba(90, 138, 90, 0.25)",
      cardBg: "#ffffff",
      border: "#e8efe8",
      borderLight: "#f2f6f2",
      shadowColor: "rgba(42, 50, 42, 0.08)",
      navBg: "#ffffff",
      navActiveBg: "#5a8a5a",
      navActiveText: "#ffffff",
      navInactiveText: "#606860",
      navHoverBg: "#f2f6f2",
      headerGlassBg: "rgba(255, 255, 255, 0.94)",
      headerGlassBlur: "10px",
    },
  },

  // Rosetta - Soft blush with terracotta warmth
  rosetta: {
    id: "rosetta",
    name: "Rosetta",
    description: "Soft blush with terracotta",
    preview: {
      primary: "#5a3a35",
      secondary: "#fdf6f4",
      accent: "#c86850",
    },
    isDark: false,
    colors: {
      bg: "#fdf6f4",
      bgSecondary: "#f8ece8",
      bgTertiary: "#f0e0da",
      text: "#3a2420",
      textSecondary: "#5a3a35",
      textMuted: "#907068",
      coffee50: "#fdf6f4",
      coffee100: "#f8ece8",
      coffee200: "#f0e0da",
      coffee300: "#e0ccc4",
      coffee400: "#c8a8a0",
      coffee500: "#a08078",
      coffee600: "#785850",
      coffee700: "#5a3a35",
      coffee800: "#4a2c28",
      coffee900: "#3a2420",
      cream100: "#fdf6f4",
      cream200: "#f8ece8",
      cream300: "#f0e0da",
      cream400: "#e0ccc4",
      accent: "#c86850",
      accentLight: "#e08870",
      accentGlow: "rgba(200, 104, 80, 0.3)",
      cardBg: "#fffcfb",
      border: "#f0e0da",
      borderLight: "#f8ece8",
      shadowColor: "rgba(58, 36, 32, 0.1)",
      navBg: "#fffcfb",
      navActiveBg: "#c86850",
      navActiveText: "#ffffff",
      navInactiveText: "#785850",
      navHoverBg: "#f8ece8",
      headerGlassBg: "rgba(255, 252, 251, 0.92)",
      headerGlassBlur: "12px",
    },
  },

  // Cappuccino - Warm dark theme with caramel/cream tones
  cappuccino: {
    id: "cappuccino",
    name: "Cappuccino",
    description: "Warm caramel on dark espresso",
    preview: {
      primary: "#e8d4c0",
      secondary: "#201814",
      accent: "#d4a070",
    },
    isDark: true,
    colors: {
      bg: "#201814",
      bgSecondary: "#302420",
      bgTertiary: "#40342c",
      text: "#f5ebe0",
      textSecondary: "#e0c8b0",
      textMuted: "#c0a088",
      coffee50: "#f5ebe0",
      coffee100: "#e8d4c0",
      coffee200: "#d4b898",
      coffee300: "#c0a088",
      coffee400: "#a08060",
      coffee500: "#806048",
      coffee600: "#604838",
      coffee700: "#40342c",
      coffee800: "#302420",
      coffee900: "#201814",
      cream100: "#40342c",
      cream200: "#504438",
      cream300: "#605448",
      cream400: "#706458",
      accent: "#d4a070",
      accentLight: "#e8c090",
      accentGlow: "rgba(212, 160, 112, 0.35)",
      cardBg: "#3a2c24",
      border: "#504438",
      borderLight: "#40342c",
      shadowColor: "rgba(0, 0, 0, 0.4)",
      navBg: "#201814",
      navActiveBg: "#d4a070",
      navActiveText: "#201814",
      navInactiveText: "#c0a088",
      navHoverBg: "#302420",
      headerGlassBg: "rgba(32, 24, 20, 0.95)",
      headerGlassBlur: "12px",
    },
  },

  // Cortado - Lighter dark theme with warm milk chocolate tones
  cortado: {
    id: "cortado",
    name: "Cortado",
    description: "Soft dark with milky warmth",
    preview: {
      primary: "#f0e0d0",
      secondary: "#3a3230",
      accent: "#c8956c",
    },
    isDark: true,
    colors: {
      bg: "#3a3230",
      bgSecondary: "#4a423e",
      bgTertiary: "#5a524c",
      text: "#f5ece4",
      textSecondary: "#e0d0c4",
      textMuted: "#b8a898",
      coffee50: "#f5ece4",
      coffee100: "#f0e0d0",
      coffee200: "#e0d0c0",
      coffee300: "#c8b8a8",
      coffee400: "#a89888",
      coffee500: "#887868",
      coffee600: "#686058",
      coffee700: "#5a524c",
      coffee800: "#4a423e",
      coffee900: "#3a3230",
      cream100: "#5a524c",
      cream200: "#686058",
      cream300: "#78706a",
      cream400: "#88807a",
      accent: "#c8956c",
      accentLight: "#e0b090",
      accentGlow: "rgba(200, 149, 108, 0.35)",
      cardBg: "#48403c",
      border: "#5a524c",
      borderLight: "#4a423e",
      shadowColor: "rgba(0, 0, 0, 0.3)",
      navBg: "#3a3230",
      navActiveBg: "#c8956c",
      navActiveText: "#3a3230",
      navInactiveText: "#b8a898",
      navHoverBg: "#4a423e",
      headerGlassBg: "rgba(58, 50, 48, 0.92)",
      headerGlassBlur: "12px",
    },
  },

  // FIXED: Roasted is now a proper dark theme
  roasted: {
    id: "roasted",
    name: "Roasted",
    description: "Rich dark coffee atmosphere",
    preview: {
      primary: "#e8d4c4",
      secondary: "#130c09",
      accent: "#e89050",
    },
    isDark: true,
    colors: {
      bg: "#130c09",
      bgSecondary: "#3d2820",
      bgTertiary: "#4a3428",
      text: "#f5ebe5",
      textSecondary: "#d4c0b0",
      textMuted: "#a08878",
      coffee50: "#f5ebe5",
      coffee100: "#e8d4c4",
      coffee200: "#d4b8a0",
      coffee300: "#a08878",
      coffee400: "#785848",
      coffee500: "#5a4038",
      coffee600: "#4a3428",
      coffee700: "#3d2820",
      coffee800: "#130c09",
      coffee900: "#1a100a",
      cream100: "#3d2820",
      cream200: "#4a3428",
      cream300: "#5a4438",
      cream400: "#6a5448",
      accent: "#e89050",
      accentLight: "#f0b080",
      accentGlow: "rgba(232, 144, 80, 0.4)",
      cardBg: "#3a2518",
      border: "#5a4038",
      borderLight: "#4a3428",
      shadowColor: "rgba(0, 0, 0, 0.4)",
      navBg: "#130c09",
      navActiveBg: "#e89050",
      navActiveText: "#1a100a",
      navInactiveText: "#a08878",
      navHoverBg: "#3d2820",
      headerGlassBg: "rgba(42, 24, 16, 0.95)",
      headerGlassBlur: "12px",
    },
  },

  "dark-roast": {
    id: "dark-roast",
    name: "Dark Roast",
    description: "Deep and intense dark mode",
    preview: {
      primary: "#f5dcc8",
      secondary: "#141210",
      accent: "#d4703c",
    },
    isDark: true,
    colors: {
      bg: "#0e0c0a",
      bgSecondary: "#181410",
      bgTertiary: "#241e18",
      text: "#f5e8d8",
      textSecondary: "#d4c4a8",
      textMuted: "#a89878",
      coffee50: "#f5e8d8",
      coffee100: "#d4c4a8",
      coffee200: "#b0a088",
      coffee300: "#a89878",
      coffee400: "#786850",
      coffee500: "#584838",
      coffee600: "#352818",
      coffee700: "#241e18",
      coffee800: "#181410",
      coffee900: "#0e0c0a",
      cream100: "#241e18",
      cream200: "#302820",
      cream300: "#403830",
      cream400: "#504840",
      accent: "#d4703c",
      accentLight: "#e89868",
      accentGlow: "rgba(212, 112, 60, 0.4)",
      cardBg: "#1a1614",
      border: "#352818",
      borderLight: "#241e18",
      shadowColor: "rgba(0, 0, 0, 0.5)",
      navBg: "#141210",
      navActiveBg: "#d4703c",
      navActiveText: "#0e0c0a",
      navInactiveText: "#a89878",
      navHoverBg: "#241e18",
      headerGlassBg: "rgba(14, 12, 10, 0.9)",
      headerGlassBlur: "14px",
    },
  },

  midnight: {
    id: "midnight",
    name: "Midnight Brew",
    description: "Elegant dark with teal accents",
    preview: {
      primary: "#e0d4c4",
      secondary: "#0c0e10",
      accent: "#4aa8a8",
    },
    isDark: true,
    colors: {
      bg: "#0a0c0e",
      bgSecondary: "#121618",
      bgTertiary: "#1a2024",
      text: "#e8e0d4",
      textSecondary: "#c8b8a0",
      textMuted: "#888078",
      coffee50: "#e8e0d4",
      coffee100: "#c8b8a0",
      coffee200: "#a89880",
      coffee300: "#888078",
      coffee400: "#585450",
      coffee500: "#383838",
      coffee600: "#222428",
      coffee700: "#1a2024",
      coffee800: "#121618",
      coffee900: "#0a0c0e",
      cream100: "#1a2024",
      cream200: "#222428",
      cream300: "#2a3038",
      cream400: "#384048",
      accent: "#4aa8a8",
      accentLight: "#78c8c8",
      accentGlow: "rgba(74, 168, 168, 0.35)",
      cardBg: "#141820",
      border: "#222428",
      borderLight: "#1a2024",
      shadowColor: "rgba(0, 0, 0, 0.6)",
      navBg: "#0e1214",
      navActiveBg: "#4aa8a8",
      navActiveText: "#0a0c0e",
      navInactiveText: "#888078",
      navHoverBg: "#1a2024",
      headerGlassBg: "rgba(10, 12, 14, 0.92)",
      headerGlassBlur: "16px",
    },
  },
};

export const themeList = Object.values(themes);

// Apply theme to document
export function applyTheme(theme: Theme) {
  const root = document.documentElement;
  const { colors } = theme;

  // Set CSS custom properties
  root.style.setProperty("--bg", colors.bg);
  root.style.setProperty("--bg-secondary", colors.bgSecondary);
  root.style.setProperty("--bg-tertiary", colors.bgTertiary);
  root.style.setProperty("--text", colors.text);
  root.style.setProperty("--text-secondary", colors.textSecondary);
  root.style.setProperty("--text-muted", colors.textMuted);

  root.style.setProperty("--coffee-50", colors.coffee50);
  root.style.setProperty("--coffee-100", colors.coffee100);
  root.style.setProperty("--coffee-200", colors.coffee200);
  root.style.setProperty("--coffee-300", colors.coffee300);
  root.style.setProperty("--coffee-400", colors.coffee400);
  root.style.setProperty("--coffee-500", colors.coffee500);
  root.style.setProperty("--coffee-600", colors.coffee600);
  root.style.setProperty("--coffee-700", colors.coffee700);
  root.style.setProperty("--coffee-800", colors.coffee800);
  root.style.setProperty("--coffee-900", colors.coffee900);

  root.style.setProperty("--cream-100", colors.cream100);
  root.style.setProperty("--cream-200", colors.cream200);
  root.style.setProperty("--cream-300", colors.cream300);
  root.style.setProperty("--cream-400", colors.cream400);

  root.style.setProperty("--accent", colors.accent);
  root.style.setProperty("--accent-light", colors.accentLight);
  root.style.setProperty("--accent-glow", colors.accentGlow);

  root.style.setProperty("--card-bg", colors.cardBg);
  root.style.setProperty("--border", colors.border);
  root.style.setProperty("--border-light", colors.borderLight);
  root.style.setProperty("--shadow-color", colors.shadowColor);

  // Navigation
  root.style.setProperty("--nav-bg", colors.navBg);
  root.style.setProperty("--nav-active-bg", colors.navActiveBg);
  root.style.setProperty("--nav-active-text", colors.navActiveText);
  root.style.setProperty("--nav-inactive-text", colors.navInactiveText);
  root.style.setProperty("--nav-hover-bg", colors.navHoverBg);

  // Header
  root.style.setProperty("--header-glass-bg", colors.headerGlassBg);
  root.style.setProperty("--header-glass-blur", colors.headerGlassBlur);

  // Set dark mode class
  if (theme.isDark) {
    root.classList.add("dark");
  } else {
    root.classList.remove("dark");
  }

  // Update meta theme-color for mobile browsers
  const metaThemeColor = document.querySelector('meta[name="theme-color"]');
  if (metaThemeColor) {
    metaThemeColor.setAttribute("content", colors.bg);
  }

  // Update Apple status bar style based on theme
  const metaStatusBar = document.querySelector(
    'meta[name="apple-mobile-web-app-status-bar-style"]'
  );
  if (metaStatusBar) {
    // black-translucent = light content (for dark backgrounds)
    // default = dark content (for light backgrounds)
    metaStatusBar.setAttribute(
      "content",
      theme.isDark ? "black-translucent" : "default"
    );
  }

  // Store preference
  localStorage.setItem("brewos-theme", theme.id);
}

// Get stored theme or default based on system preference
export function getStoredTheme(): Theme {
  const stored = localStorage.getItem("brewos-theme") as ThemeId | null;
  if (stored && themes[stored]) {
    return themes[stored];
  }

  // No stored theme - check system preference
  if (typeof window !== "undefined" && window.matchMedia) {
    const prefersDark = window.matchMedia(
      "(prefers-color-scheme: dark)"
    ).matches;
    return prefersDark ? themes.cappuccino : themes.caramel;
  }

  // Fallback to caramel (light theme)
  return themes.caramel;
}
