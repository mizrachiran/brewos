/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        // Dark admin theme inspired by modern dashboards
        "admin-bg": "#0a0a0f",
        "admin-surface": "#12121a",
        "admin-card": "#1a1a24",
        "admin-border": "#2a2a3a",
        "admin-hover": "#252530",
        "admin-accent": "#6366f1",
        "admin-accent-hover": "#818cf8",
        "admin-success": "#22c55e",
        "admin-warning": "#f59e0b",
        "admin-danger": "#ef4444",
        "admin-text": "#e2e8f0",
        "admin-text-secondary": "#94a3b8",
      },
      fontFamily: {
        sans: ["JetBrains Mono", "SF Mono", "Monaco", "monospace"],
        display: ["Inter", "system-ui", "sans-serif"],
      },
      boxShadow: {
        admin: "0 4px 20px rgba(0, 0, 0, 0.5)",
        "admin-hover": "0 8px 30px rgba(0, 0, 0, 0.6)",
      },
      animation: {
        "fade-in": "fadeIn 0.3s ease-out",
        "slide-in": "slideIn 0.3s ease-out",
        pulse: "pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite",
      },
      keyframes: {
        fadeIn: {
          "0%": { opacity: "0" },
          "100%": { opacity: "1" },
        },
        slideIn: {
          "0%": { transform: "translateY(-10px)", opacity: "0" },
          "100%": { transform: "translateY(0)", opacity: "1" },
        },
      },
    },
  },
  plugins: [],
};

