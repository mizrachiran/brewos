# BrewOS Web Interface

The BrewOS web interface is a modern React application that provides a dashboard for monitoring and controlling your espresso machine. It's designed to work both directly with the ESP32 (local mode) and through the cloud service (remote access).

## Features

- **Dashboard** - Real-time status, temperatures, pressure, and power consumption
- **Brewing** - Brew-by-weight settings and shot control
- **Scale** - BLE scale pairing and management
- **Settings** - Temperature, WiFi, MQTT, and machine configuration
- **System** - Firmware updates, logs, and diagnostics
- **About** - Device information and resources
- **Progressive Web App (PWA)** - Install on any device, works offline
- **Push Notifications** - Receive alerts when your machine needs attention

## Tech Stack

- **React 18** - UI framework
- **Vite** - Build tool and dev server
- **TypeScript** - Type safety
- **Tailwind CSS** - Styling
- **Zustand** - State management
- **React Router** - Navigation
- **Lucide React** - Icons

## Project Structure

```
src/web/
├── public/
│   ├── favicon.svg
│   └── logo.png
├── src/
│   ├── components/         # Reusable UI components
│   │   ├── Badge.tsx
│   │   ├── Button.tsx
│   │   ├── Card.tsx
│   │   ├── Gauge.tsx
│   │   ├── Input.tsx
│   │   ├── Layout.tsx
│   │   └── Toggle.tsx
│   ├── pages/              # Page components
│   │   ├── Dashboard.tsx
│   │   ├── Brewing.tsx
│   │   ├── Scale.tsx
│   │   ├── Settings.tsx
│   │   ├── System.tsx
│   │   └── About.tsx
│   ├── lib/
│   │   ├── connection.ts   # WebSocket connection manager
│   │   ├── store.ts        # Zustand state store
│   │   ├── types.ts        # TypeScript interfaces
│   │   └── utils.ts        # Helper functions
│   ├── styles/
│   │   └── index.css       # Tailwind + custom styles
│   ├── App.tsx             # Main app component
│   └── main.tsx            # Entry point
├── index.html
├── package.json
├── tailwind.config.js
├── tsconfig.json
└── vite.config.ts
```

## Development

### Prerequisites

- Node.js 18+
- npm or yarn

### Setup

```bash
cd src/web
npm install
```

### Run Development Server

```bash
npm run dev
```

The dev server runs at `http://localhost:3000` with:
- Hot module replacement
- WebSocket proxy to `ws://brewos.local/ws`
- API proxy to `http://brewos.local/api`

### Build

**For ESP32 (embedded in firmware):**

```bash
npm run build:esp32
# Output: src/esp32/data/
```

This build is optimized for size:
- Console logs removed
- Maximum minification
- Single bundle (no code splitting)

**For Cloud deployment:**

```bash
npm run build
# Output: src/web/dist/
```

This build is optimized for performance:
- Code splitting
- Vendor chunk separation
- Source maps

## Connection Modes

The web interface supports two connection modes:

### Local Mode (ESP32 Direct)

When accessing via `http://brewos.local`, the app connects directly to the ESP32:

```
Browser ──WebSocket──► ESP32 (ws://brewos.local/ws)
```

### Cloud Mode (Remote Access)

When accessing via the cloud service, messages are relayed:

```
Browser ──WebSocket──► Cloud ──WebSocket──► ESP32
```

The connection mode is determined automatically based on how the app is initialized.

## State Management

The app uses Zustand for state management. The store (`src/lib/store.ts`) maintains:

- Connection state
- Machine status (state, mode, temperatures, pressure)
- Scale status
- Brew-by-weight settings
- WiFi/MQTT configuration
- System logs and alerts

### Subscribing to State

```tsx
import { useStore } from '@/lib/store';

function MyComponent() {
  const temps = useStore((s) => s.temps);
  const machine = useStore((s) => s.machine);
  
  return (
    <div>
      Brew: {temps.brew.current}°C / {temps.brew.setpoint}°C
    </div>
  );
}
```

### Sending Commands

```tsx
import { getConnection } from '@/lib/connection';

function handleSetTemp() {
  getConnection()?.sendCommand('set_temp', { 
    boiler: 'brew', 
    temp: 93.5 
  });
}
```

## Progressive Web App

The web interface is a fully functional PWA. See [PWA Documentation](./PWA.md) for:
- Installation instructions
- Offline support
- Service worker details
- Troubleshooting

## Push Notifications

Receive real-time notifications when your machine needs attention. See [Push Notifications Documentation](./Push_Notifications.md) for:
- Setup instructions
- Supported notification types
- API reference
- Troubleshooting

## WebSocket Protocol

See [WebSocket Protocol](./WebSocket_Protocol.md) for message format details.

## Storybook

Browse and test all UI components interactively using Storybook:

```bash
# From project root
./src/scripts/run_storybook.sh

# Or from web directory
cd src/web
npm run storybook
```

Storybook runs at http://localhost:6006 and provides:

- **🎨 Theme Switcher** - Preview all 10 themes using the toolbar (paintbrush icon)
- **📚 Component Documentation** - Auto-generated docs with usage examples
- **🔧 Interactive Controls** - Adjust component props in real-time
- **📱 Viewport Testing** - Test responsive behavior at different sizes

### Available Stories

| Category | Components |
|----------|------------|
| **Core** | Button, Badge, Card, Input, Toggle, Gauge, Loading, Toast, Logo |
| **Dashboard** | MachineStatusCard, TemperatureGauges, PowerCard |
| **Settings** | SettingsSection, ThemeSettings |
| **Design System** | Theme Showcase, Color Palettes |

### Building Storybook

```bash
# Build static site
npm run build-storybook

# Output: src/web/storybook-static/
```

## Design System

The UI uses a coffee-themed design system with 10 customizable themes.

### Themes

**Light Themes:**
- ☀️ Caramel - Warm amber and honey tones (default)
- ☀️ Classic Coffee - Warm and inviting browns
- ☀️ Mocha - Rich chocolate with purple hints
- ☀️ Espresso - Deep and bold with golden accent
- ☀️ Latte - Light, airy, and creamy
- ☀️ Warm Mocha - Soft mocha with warm accents

**Dark Themes:**
- 🌙 Cortado - Soft dark with milky warmth (lightest dark theme)
- 🌙 Cappuccino - Warm caramel on dark espresso
- 🌙 Roasted - Rich dark coffee atmosphere
- 🌙 Dark Roast - Deep and intense dark mode
- 🌙 Midnight Brew - Elegant dark with teal accents

Theme definitions are in `src/lib/themes.ts` and managed via `useThemeStore`.

### Colors

All colors use CSS variables for theme-awareness:

- `coffee-50` to `coffee-900` - Primary brown palette
- `cream-100` to `cream-400` - Secondary light tones
- `accent`, `accent-light`, `accent-glow` - Highlight colors
- `bg`, `bg-secondary`, `bg-tertiary` - Background layers
- `text`, `text-secondary`, `text-muted` - Text colors

### Components

Pre-styled components are available:

- `.card` - Container with border and shadow
- `.btn`, `.btn-primary`, `.btn-secondary` - Buttons
- `.input` - Form inputs
- `.badge`, `.badge-success`, etc. - Status badges

## Adding New Pages

1. Create component in `src/pages/NewPage.tsx`
2. Add route in `src/App.tsx`
3. Add navigation link in `src/components/Layout.tsx`

## Environment Variables

The build uses compile-time variables:

- `__ESP32__` - `true` when building for ESP32
- `__CLOUD__` - `true` when building for cloud

```tsx
if (__ESP32__) {
  // ESP32-specific code
}
```

