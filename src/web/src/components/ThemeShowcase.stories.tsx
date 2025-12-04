import type { Meta, StoryObj } from "@storybook/react";
import { themeList } from "../lib/themes";
import { Button } from "./Button";
import { Badge } from "./Badge";
import { Card, CardHeader, CardTitle } from "./Card";
import { Input } from "./Input";
import { Toggle } from "./Toggle";
import { Logo } from "./Logo";
import { useState } from "react";
import {
  Coffee,
  Thermometer,
  Settings,
  Check,
  AlertTriangle,
} from "lucide-react";

const meta: Meta = {
  title: "Design System/Theme Showcase",
  tags: ["autodocs"],
};

export default meta;
type Story = StoryObj;

// Component showcase for all themes
function ComponentShowcase() {
  const [toggle1, setToggle1] = useState(true);
  const [toggle2, setToggle2] = useState(false);

  return (
    <div className="space-y-8">
      {/* Logo */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Logo</h2>
        <div className="flex gap-6 items-end flex-wrap">
          <Logo size="sm" />
          <Logo size="md" />
          <Logo size="lg" />
        </div>
      </section>

      {/* Buttons */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Buttons</h2>
        <div className="flex flex-wrap gap-3">
          <Button variant="primary">Primary</Button>
          <Button variant="secondary">Secondary</Button>
          <Button variant="accent">Accent</Button>
          <Button variant="ghost">Ghost</Button>
          <Button variant="danger">Danger</Button>
          <Button loading>Loading</Button>
          <Button disabled>Disabled</Button>
        </div>
      </section>

      {/* Badges */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Badges</h2>
        <div className="flex flex-wrap gap-3">
          <Badge variant="default">Default</Badge>
          <Badge variant="success">
            <Check className="w-3 h-3" />
            Online
          </Badge>
          <Badge variant="warning">
            <AlertTriangle className="w-3 h-3" />
            Heating
          </Badge>
          <Badge variant="error">Offline</Badge>
          <Badge variant="info">Update</Badge>
        </div>
      </section>

      {/* Cards */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Cards</h2>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          <Card>
            <CardHeader>
              <CardTitle icon={<Thermometer className="w-5 h-5" />}>
                Temperature
              </CardTitle>
            </CardHeader>
            <div className="text-3xl font-bold text-theme">93°C</div>
            <p className="text-theme-muted text-sm mt-1">Target: 93°C</p>
          </Card>

          <Card>
            <CardHeader action={<Badge variant="success">Ready</Badge>}>
              <CardTitle icon={<Coffee className="w-5 h-5" />}>Status</CardTitle>
            </CardHeader>
            <p className="text-theme-secondary">Machine is ready to brew.</p>
          </Card>

          <Card>
            <CardHeader action={<Button size="sm">Configure</Button>}>
              <CardTitle icon={<Settings className="w-5 h-5" />}>
                Settings
              </CardTitle>
            </CardHeader>
            <p className="text-theme-muted">Customize your brew experience.</p>
          </Card>
        </div>
      </section>

      {/* Inputs */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Inputs</h2>
        <div className="max-w-md space-y-4">
          <Input label="Brew Temperature" placeholder="93" unit="°C" />
          <Input
            label="Pre-infusion"
            placeholder="3"
            unit="sec"
            hint="Time to pre-wet the coffee"
          />
          <Input
            label="Invalid Field"
            defaultValue="120"
            error="Temperature must be under 100°C"
          />
        </div>
      </section>

      {/* Toggles */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Toggles</h2>
        <div className="space-y-4">
          <div className="flex items-center justify-between p-4 bg-theme-card rounded-xl border border-theme">
            <div>
              <div className="font-medium text-theme">Power</div>
              <div className="text-sm text-theme-muted">Machine power</div>
            </div>
            <Toggle checked={toggle1} onChange={setToggle1} />
          </div>
          <div className="flex items-center justify-between p-4 bg-theme-card rounded-xl border border-theme">
            <div>
              <div className="font-medium text-theme">Eco Mode</div>
              <div className="text-sm text-theme-muted">Reduce power usage</div>
            </div>
            <Toggle checked={toggle2} onChange={setToggle2} />
          </div>
        </div>
      </section>

      {/* Colors */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Theme Colors</h2>
        <div className="space-y-4">
          <div>
            <h3 className="text-sm font-medium text-theme-muted mb-2">
              Coffee Palette
            </h3>
            <div className="flex gap-1 flex-wrap">
              <div className="w-12 h-12 rounded-lg bg-coffee-50 border border-theme" />
              <div className="w-12 h-12 rounded-lg bg-coffee-100" />
              <div className="w-12 h-12 rounded-lg bg-coffee-200" />
              <div className="w-12 h-12 rounded-lg bg-coffee-300" />
              <div className="w-12 h-12 rounded-lg bg-coffee-400" />
              <div className="w-12 h-12 rounded-lg bg-coffee-500" />
              <div className="w-12 h-12 rounded-lg bg-coffee-600" />
              <div className="w-12 h-12 rounded-lg bg-coffee-700" />
              <div className="w-12 h-12 rounded-lg bg-coffee-800" />
              <div className="w-12 h-12 rounded-lg bg-coffee-900" />
            </div>
          </div>
          <div>
            <h3 className="text-sm font-medium text-theme-muted mb-2">Accent</h3>
            <div className="flex gap-1">
              <div className="w-12 h-12 rounded-lg bg-accent" />
              <div className="w-12 h-12 rounded-lg bg-accent-light" />
            </div>
          </div>
          <div>
            <h3 className="text-sm font-medium text-theme-muted mb-2">
              Backgrounds
            </h3>
            <div className="flex gap-1">
              <div className="w-12 h-12 rounded-lg bg-theme border border-theme" />
              <div className="w-12 h-12 rounded-lg bg-theme-secondary border border-theme" />
              <div className="w-12 h-12 rounded-lg bg-theme-tertiary border border-theme" />
              <div className="w-12 h-12 rounded-lg bg-theme-card border border-theme" />
            </div>
          </div>
        </div>
      </section>

      {/* Text */}
      <section>
        <h2 className="text-lg font-semibold text-theme mb-4">Typography</h2>
        <div className="space-y-2">
          <p className="text-4xl font-bold text-theme">Heading 1</p>
          <p className="text-2xl font-semibold text-theme">Heading 2</p>
          <p className="text-xl font-medium text-theme">Heading 3</p>
          <p className="text-base text-theme">Body text - primary</p>
          <p className="text-base text-theme-secondary">Body text - secondary</p>
          <p className="text-sm text-theme-muted">Small text - muted</p>
          <p className="text-accent font-semibold">Accent text</p>
        </div>
      </section>
    </div>
  );
}

export const Showcase: Story = {
  render: () => <ComponentShowcase />,
};

// Theme preview cards
function ThemeGrid() {
  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-xl font-bold text-theme mb-4">Available Themes</h2>
        <p className="text-theme-muted mb-6">
          Use the theme selector in the toolbar above to switch between themes.
          Below is a preview of all available themes.
        </p>
      </div>

      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-5 gap-4">
        {themeList.map((theme) => (
          <div
            key={theme.id}
            className="rounded-xl overflow-hidden border border-theme"
          >
            <div
              className="h-24 flex items-center justify-center"
              style={{ backgroundColor: theme.colors.bg }}
            >
              <div
                className="w-12 h-12 rounded-full"
                style={{ backgroundColor: theme.colors.accent }}
              />
            </div>
            <div className="p-3 bg-theme-card">
              <div className="flex items-center gap-2">
                <span className="text-lg">
                  {theme.isDark ? "🌙" : "☀️"}
                </span>
                <span className="font-semibold text-theme text-sm">
                  {theme.name}
                </span>
              </div>
              <p className="text-xs text-theme-muted mt-1">{theme.description}</p>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

export const ThemeOverview: Story = {
  render: () => <ThemeGrid />,
};

// Dark/Light comparison
function DarkLightComparison() {
  const lightThemes = themeList.filter((t) => !t.isDark);
  const darkThemes = themeList.filter((t) => t.isDark);

  return (
    <div className="space-y-8">
      <div>
        <h2 className="text-xl font-bold text-theme mb-2 flex items-center gap-2">
          ☀️ Light Themes ({lightThemes.length})
        </h2>
        <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
          {lightThemes.map((theme) => (
            <div
              key={theme.id}
              className="p-4 rounded-xl border-2"
              style={{
                backgroundColor: theme.colors.cardBg,
                borderColor: theme.colors.border,
              }}
            >
              <div className="flex items-center gap-3 mb-3">
                <div
                  className="w-8 h-8 rounded-full"
                  style={{ backgroundColor: theme.colors.accent }}
                />
                <span
                  className="font-bold"
                  style={{ color: theme.colors.text }}
                >
                  {theme.name}
                </span>
              </div>
              <div className="flex gap-1">
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.bg }}
                />
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.bgSecondary }}
                />
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.bgTertiary }}
                />
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.accent }}
                />
              </div>
            </div>
          ))}
        </div>
      </div>

      <div>
        <h2 className="text-xl font-bold text-theme mb-2 flex items-center gap-2">
          🌙 Dark Themes ({darkThemes.length})
        </h2>
        <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
          {darkThemes.map((theme) => (
            <div
              key={theme.id}
              className="p-4 rounded-xl border-2"
              style={{
                backgroundColor: theme.colors.cardBg,
                borderColor: theme.colors.border,
              }}
            >
              <div className="flex items-center gap-3 mb-3">
                <div
                  className="w-8 h-8 rounded-full"
                  style={{ backgroundColor: theme.colors.accent }}
                />
                <span
                  className="font-bold"
                  style={{ color: theme.colors.text }}
                >
                  {theme.name}
                </span>
              </div>
              <div className="flex gap-1">
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.bg }}
                />
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.bgSecondary }}
                />
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.bgTertiary }}
                />
                <div
                  className="w-6 h-6 rounded"
                  style={{ backgroundColor: theme.colors.accent }}
                />
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

export const LightVsDark: Story = {
  render: () => <DarkLightComparison />,
};

