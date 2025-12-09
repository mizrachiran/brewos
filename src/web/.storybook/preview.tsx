import type { Preview, Decorator } from "@storybook/react";
import { useEffect } from "react";
import { themes, ThemeId, applyTheme, themeList } from "../src/lib/themes";
import { useThemeStore } from "../src/lib/themeStore";
import "../src/styles/index.css";

// Custom viewports for mobile testing
const customViewports = {
  // iPhone 12 / 12 Pro / 13 / 13 Pro / 14
  iPhone12Portrait: {
    name: "iPhone 12 Portrait",
    styles: {
      width: "390px",
      height: "844px",
    },
    type: "mobile" as const,
  },
  iPhone12Landscape: {
    name: "iPhone 12 Landscape",
    styles: {
      width: "844px",
      height: "390px",
    },
    type: "mobile" as const,
  },
  // iPhone 14 Pro Max / 15 Pro Max
  iPhone14ProMaxPortrait: {
    name: "iPhone 14 Pro Max Portrait",
    styles: {
      width: "430px",
      height: "932px",
    },
    type: "mobile" as const,
  },
  iPhone14ProMaxLandscape: {
    name: "iPhone 14 Pro Max Landscape",
    styles: {
      width: "932px",
      height: "430px",
    },
    type: "mobile" as const,
  },
  // Small phone (iPhone SE / older devices)
  iPhoneSEPortrait: {
    name: "iPhone SE Portrait",
    styles: {
      width: "375px",
      height: "667px",
    },
    type: "mobile" as const,
  },
  iPhoneSELandscape: {
    name: "iPhone SE Landscape",
    styles: {
      width: "667px",
      height: "375px",
    },
    type: "mobile" as const,
  },
  // Tablet
  iPadPortrait: {
    name: "iPad Portrait",
    styles: {
      width: "768px",
      height: "1024px",
    },
    type: "tablet" as const,
  },
  iPadLandscape: {
    name: "iPad Landscape",
    styles: {
      width: "1024px",
      height: "768px",
    },
    type: "tablet" as const,
  },
};

// Theme decorator that applies the selected theme
const ThemeDecorator: Decorator = (Story, context) => {
  const selectedTheme = context.globals.theme as ThemeId;
  const setTheme = useThemeStore((s) => s.setTheme);

  useEffect(() => {
    const theme = themes[selectedTheme] || themes.caramel;
    applyTheme(theme);
    // Also update the theme store so components like Logo can read from it
    setTheme(selectedTheme || "caramel");
    
    // Override the global overflow:hidden for Storybook
    document.documentElement.style.overflow = "auto";
    document.body.style.overflow = "auto";
    document.documentElement.style.height = "auto";
    document.body.style.height = "auto";
    
    return () => {
      // Cleanup
      document.documentElement.style.overflow = "";
      document.body.style.overflow = "";
      document.documentElement.style.height = "";
      document.body.style.height = "";
    };
  }, [selectedTheme, setTheme]);

  return (
    <div className="min-h-full bg-theme p-6 transition-colors duration-300">
      <Story />
    </div>
  );
};

// Wrapper for components that need router context
const RouterDecorator: Decorator = (Story) => {
  return <Story />;
};

const preview: Preview = {
  parameters: {
    actions: { argTypesRegex: "^on[A-Z].*" },
    controls: {
      matchers: {
        color: /(background|color)$/i,
        date: /Date$/i,
      },
    },
    backgrounds: { disable: true }, // We use our own theme system
    layout: "fullscreen",
    viewport: {
      viewports: customViewports,
      defaultViewport: "responsive",
    },
  },
  globalTypes: {
    theme: {
      name: "Theme",
      description: "BrewOS Theme Selector",
      defaultValue: "caramel",
      toolbar: {
        icon: "paintbrush",
        items: themeList.map((t) => ({
          value: t.id,
          title: `${t.isDark ? "🌙" : "☀️"} ${t.name}`,
          right: t.description,
        })),
        dynamicTitle: true,
      },
    },
  },
  decorators: [ThemeDecorator, RouterDecorator],
};

export default preview;

