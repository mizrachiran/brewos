import { useStore } from "@/lib/store";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Logo } from "@/components/Logo";
import {
  Cpu,
  Code,
  Heart,
  Github,
  ExternalLink,
  Bug,
  Lightbulb,
  MessageSquare,
  Coffee,
  Zap,
  Shield,
  Monitor,
} from "lucide-react";

export function AboutSection() {
  const esp32 = useStore((s) => s.esp32);
  const pico = useStore((s) => s.pico);

  const GITHUB_REPO = "https://github.com/mizrachiran/brewos";

  // Use build-time version constant
  const uiVersion = __VERSION__;

  return (
    <div className="space-y-6">
      {/* Hero */}
      <Card className="relative overflow-hidden text-center bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 text-white border-0">
        {/* Decorative background pattern */}
        <div className="absolute inset-0 opacity-20">
          <div className="absolute top-4 left-4 w-32 h-32 border border-white/50 rounded-full" />
          <div className="absolute bottom-4 right-4 w-24 h-24 border border-white/50 rounded-full" />
          <div className="absolute top-1/2 left-1/4 w-16 h-16 border border-white/50 rounded-full" />
        </div>

        <div className="relative py-10">
          <div className="flex justify-center mb-6">
            <div className="relative">
              <Logo size="xl" forceLight />
              <div className="absolute -top-1 -right-1 w-4 h-4 bg-green-400 rounded-full animate-pulse" />
            </div>
          </div>
          <h1 className="text-2xl font-bold mb-2">BrewOS</h1>
          <p className="text-cream-300 mb-6 max-w-md mx-auto">
            Open-source espresso machine controller for precision brewing
          </p>
          <div className="flex flex-wrap items-center justify-center gap-3 text-sm">
            <div className="flex items-center gap-2 bg-white/10 px-3 py-1.5 rounded-full">
              <Monitor className="w-4 h-4 text-amber-400" />
              <span>UI: {uiVersion}</span>
            </div>
            <div className="flex items-center gap-2 bg-white/10 px-3 py-1.5 rounded-full">
              <Cpu className="w-4 h-4 text-amber-400" />
              <span>Network: {esp32.version || "—"}</span>
            </div>
            <div className="flex items-center gap-2 bg-white/10 px-3 py-1.5 rounded-full">
              <Zap className="w-4 h-4 text-amber-400" />
              <span>Controller: {pico.version || "—"}</span>
            </div>
          </div>
        </div>
      </Card>

      {/* Features */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <AboutFeatureCard
          icon={<Cpu className="w-6 h-6" />}
          title="Dual-Core Control"
          description="Network controller for connectivity, machine controller for real-time brewing"
          color="amber"
        />
        <AboutFeatureCard
          icon={<Code className="w-6 h-6" />}
          title="Fully Open Source"
          description="Hardware and software designs available on GitHub"
          color="emerald"
        />
        <AboutFeatureCard
          icon={<Shield className="w-6 h-6" />}
          title="Safety First"
          description="Class-B certified safety monitoring and controls"
          color="blue"
        />
      </div>

      {/* Feedback & Support */}
      <Card>
        <CardHeader>
          <CardTitle icon={<MessageSquare className="w-5 h-5" />}>
            Feedback & Support
          </CardTitle>
        </CardHeader>

        <p className="text-theme-muted text-sm mb-4">
          Help us improve BrewOS! Report issues or suggest new features through
          GitHub.
        </p>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
          <FeedbackCard
            icon={<Bug className="w-5 h-5" />}
            title="Report a Bug"
            description="Found something that's not working right?"
            href={`${GITHUB_REPO}/issues/new?labels=bug&template=bug_report.md&title=%5BBug%5D%3A+`}
            variant="bug"
          />
          <FeedbackCard
            icon={<Lightbulb className="w-5 h-5" />}
            title="Request a Feature"
            description="Have an idea to make BrewOS better?"
            href={`${GITHUB_REPO}/issues/new?labels=enhancement&template=feature_request.md&title=%5BFeature%5D%3A+`}
            variant="feature"
          />
        </div>
      </Card>

      {/* Resources */}
      <Card>
        <CardHeader>
          <CardTitle>Resources</CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
          <AboutLinkCard
            icon={<Github className="w-5 h-5" />}
            title="GitHub Repository"
            description="Source code and documentation"
            href={GITHUB_REPO}
          />
          <AboutLinkCard
            icon={<ExternalLink className="w-5 h-5" />}
            title="Documentation"
            description="Setup guides and API reference"
            href="https://brewos.io"
          />
        </div>
      </Card>

      {/* Credits */}
      <Card className="text-center bg-gradient-to-r from-theme-card to-theme-secondary">
        <div className="flex items-center justify-center gap-2 mb-3">
          <Coffee className="w-5 h-5 text-amber-600" />
          <span className="font-medium text-theme">Community Driven</span>
          <Coffee className="w-5 h-5 text-amber-600" />
        </div>
        <p className="text-theme-muted mb-2">
          Made with{" "}
          <Heart className="w-4 h-4 inline text-red-500 animate-pulse" /> for
          espresso lovers
        </p>
        <p className="text-sm text-theme-muted">
          © {new Date().getFullYear()} BrewOS Contributors
        </p>
      </Card>
    </div>
  );
}

interface AboutFeatureCardProps {
  icon: React.ReactNode;
  title: string;
  description: string;
  color?: "amber" | "emerald" | "blue";
}

function AboutFeatureCard({
  icon,
  title,
  description,
  color = "amber",
}: AboutFeatureCardProps) {
  const colorClasses = {
    amber: "bg-amber-500/10 text-amber-600 dark:text-amber-400",
    emerald: "bg-emerald-500/10 text-emerald-600 dark:text-emerald-400",
    blue: "bg-blue-500/10 text-blue-600 dark:text-blue-400",
  };

  return (
    <Card className="text-center hover:shadow-lg transition-shadow">
      <div
        className={`w-14 h-14 rounded-2xl flex items-center justify-center mx-auto mb-4 ${colorClasses[color]}`}
      >
        {icon}
      </div>
      <h3 className="font-semibold text-theme mb-2">{title}</h3>
      <p className="text-sm text-theme-muted leading-relaxed">{description}</p>
    </Card>
  );
}

interface FeedbackCardProps {
  icon: React.ReactNode;
  title: string;
  description: string;
  href: string;
  variant: "bug" | "feature";
}

function FeedbackCard({
  icon,
  title,
  description,
  href,
  variant,
}: FeedbackCardProps) {
  const variantClasses = {
    bug: "hover:border-red-500/50 hover:bg-red-500/5 group-hover:text-red-500 group-hover:bg-red-500/10",
    feature:
      "hover:border-amber-500/50 hover:bg-amber-500/5 group-hover:text-amber-500 group-hover:bg-amber-500/10",
  };

  const iconBgClasses = {
    bug: "bg-red-500/10 text-red-500",
    feature: "bg-amber-500/10 text-amber-500",
  };

  return (
    <a
      href={href}
      target="_blank"
      rel="noopener noreferrer"
      className={`group flex items-start gap-4 p-4 bg-theme-secondary rounded-xl border-2 border-transparent transition-all ${variantClasses[variant]}`}
    >
      <div
        className={`w-12 h-12 rounded-xl flex items-center justify-center flex-shrink-0 transition-colors ${iconBgClasses[variant]}`}
      >
        {icon}
      </div>
      <div className="min-w-0">
        <h4 className="font-semibold text-theme group-hover:text-accent transition-colors flex items-center gap-2">
          {title}
          <ExternalLink className="w-3.5 h-3.5 opacity-0 group-hover:opacity-100 transition-opacity" />
        </h4>
        <p className="text-sm text-theme-muted mt-0.5">{description}</p>
      </div>
    </a>
  );
}

interface AboutLinkCardProps {
  icon: React.ReactNode;
  title: string;
  description: string;
  href: string;
}

function AboutLinkCard({ icon, title, description, href }: AboutLinkCardProps) {
  return (
    <a
      href={href}
      target="_blank"
      rel="noopener noreferrer"
      className="flex items-start gap-4 p-4 bg-theme-secondary rounded-xl hover:bg-theme-tertiary transition-colors group"
    >
      <div className="w-10 h-10 bg-theme-card rounded-lg flex items-center justify-center text-theme-secondary group-hover:text-accent transition-colors">
        {icon}
      </div>
      <div>
        <h4 className="font-semibold text-theme group-hover:text-accent transition-colors">
          {title}
        </h4>
        <p className="text-sm text-theme-muted">{description}</p>
      </div>
    </a>
  );
}
