/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  darkMode: "class",
  theme: {
    extend: {
      screens: {
        xs: "390px",
      },
      colors: {
        // Theme-aware colors using CSS variables
        coffee: {
          50: "var(--coffee-50)",
          100: "var(--coffee-100)",
          200: "var(--coffee-200)",
          300: "var(--coffee-300)",
          400: "var(--coffee-400)",
          500: "var(--coffee-500)",
          600: "var(--coffee-600)",
          700: "var(--coffee-700)",
          800: "var(--coffee-800)",
          900: "var(--coffee-900)",
        },
        cream: {
          100: "var(--cream-100)",
          200: "var(--cream-200)",
          300: "var(--cream-300)",
          400: "var(--cream-400)",
        },
        accent: {
          DEFAULT: "var(--accent)",
          light: "var(--accent-light)",
          glow: "var(--accent-glow)",
        },
      },
      backgroundColor: {
        theme: {
          DEFAULT: "var(--bg)",
          secondary: "var(--bg-secondary)",
          tertiary: "var(--bg-tertiary)",
          card: "var(--card-bg)",
        },
      },
      textColor: {
        theme: {
          DEFAULT: "var(--text)",
          secondary: "var(--text-secondary)",
          muted: "var(--text-muted)",
        },
      },
      borderColor: {
        theme: {
          DEFAULT: "var(--border)",
          light: "var(--border-light)",
        },
      },
      fontFamily: {
        sans: ["Plus Jakarta Sans", "system-ui", "sans-serif"],
        mono: ["JetBrains Mono", "SF Mono", "monospace"],
      },
      animation: {
        "pulse-slow": "pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite",
        "spin-slow": "spin 2s linear infinite",
      },
      boxShadow: {
        soft: "0 4px 20px var(--shadow-color)",
        card: "0 2px 12px var(--shadow-color)",
        glow: "0 0 20px var(--accent-glow)",
      },
    },
  },
  plugins: [],
};
