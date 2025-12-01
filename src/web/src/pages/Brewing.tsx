import { useState, useEffect } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import { PageHeader } from '@/components/PageHeader';
import { Coffee, Scale, Timer, Droplet } from 'lucide-react';
import { formatDuration } from '@/lib/utils';

export function Brewing() {
  const bbw = useStore((s) => s.bbw);
  const shot = useStore((s) => s.shot);
  const scale = useStore((s) => s.scale);

  // Local form state
  const [formState, setFormState] = useState(bbw);
  const [shotTime, setShotTime] = useState(0);

  // Sync with store
  useEffect(() => {
    setFormState(bbw);
  }, [bbw]);

  // Shot timer
  useEffect(() => {
    let interval: ReturnType<typeof setInterval>;
    if (shot.active) {
      interval = setInterval(() => {
        setShotTime(Date.now() - shot.startTime);
      }, 100);
    }
    return () => clearInterval(interval);
  }, [shot.active, shot.startTime]);

  const ratio = formState.doseWeight > 0 
    ? (formState.targetWeight / formState.doseWeight).toFixed(1) 
    : '0.0';

  const saveSettings = () => {
    getConnection()?.sendCommand('set_bbw', { ...formState });
  };

  const tareScale = () => {
    getConnection()?.sendCommand('tare');
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <PageHeader
        title="Brewing"
        subtitle="Configure brew-by-weight settings"
      />

      {/* Active Shot */}
      {shot.active && (
        <Card className="bg-gradient-to-br from-accent/10 to-accent/5 border-accent/30">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-xl font-bold text-theme">Active Shot</h2>
            <span className="text-3xl font-mono font-bold text-theme">
              {formatDuration(shotTime)}
            </span>
          </div>
          
          <div className="grid grid-cols-2 gap-6 mb-4">
            <div className="text-center">
              <div className="text-4xl font-bold text-theme tabular-nums">
                {shot.weight.toFixed(1)}
              </div>
              <div className="text-sm text-theme-muted">grams</div>
            </div>
            <div className="text-center">
              <div className="text-4xl font-bold text-theme tabular-nums">
                {shot.flowRate.toFixed(1)}
              </div>
              <div className="text-sm text-theme-muted">g/s flow</div>
            </div>
          </div>

          <div className="relative h-3 bg-theme-secondary rounded-full overflow-hidden">
            <div
              className="absolute inset-y-0 left-0 rounded-full bg-gradient-to-r from-amber-400 to-accent transition-all duration-200"
              style={{ width: `${Math.min(100, (shot.weight / bbw.targetWeight) * 100)}%` }}
            />
          </div>
          <p className="text-sm text-theme-muted mt-2">
            Target: {bbw.targetWeight}g
          </p>
        </Card>
      )}

      {/* BBW Settings */}
      <Card>
        <CardHeader 
          action={
            <Toggle 
              checked={formState.enabled}
              onChange={(enabled) => setFormState({ ...formState, enabled })}
              label="Enable"
            />
          }
        >
          <CardTitle icon={<Coffee className="w-5 h-5" />}>
            Brew-by-Weight
          </CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
          <Input
            label="Dose Weight"
            type="number"
            min={10}
            max={30}
            step={0.1}
            value={formState.doseWeight}
            onChange={(e) => setFormState({ ...formState, doseWeight: parseFloat(e.target.value) || 0 })}
            unit="g"
            hint="Coffee grounds"
          />
          
          <Input
            label="Target Weight"
            type="number"
            min={20}
            max={80}
            step={0.1}
            value={formState.targetWeight}
            onChange={(e) => setFormState({ ...formState, targetWeight: parseFloat(e.target.value) || 0 })}
            unit="g"
            hint="Desired output"
          />

          <div className="space-y-1.5">
            <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted">
              Ratio
            </label>
            <div className="h-[50px] flex items-center justify-center text-2xl font-bold text-theme">
              1:{ratio}
            </div>
          </div>

          <Input
            label="Stop Offset"
            type="number"
            min={0}
            max={10}
            step={0.5}
            value={formState.stopOffset}
            onChange={(e) => setFormState({ ...formState, stopOffset: parseFloat(e.target.value) || 0 })}
            unit="g"
            hint="For drip"
          />
        </div>

        <div className="flex items-center justify-between">
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={formState.autoTare}
              onChange={(e) => setFormState({ ...formState, autoTare: e.target.checked })}
              className="w-4 h-4 rounded border-theme text-accent focus:ring-accent"
            />
            <span className="text-sm text-theme-secondary">Auto-tare when portafilter placed</span>
          </label>

          <Button onClick={saveSettings}>
            Save Settings
          </Button>
        </div>
      </Card>

      {/* Scale Status */}
      <Card>
        <CardHeader
          action={
            <Badge variant={scale.connected ? 'success' : 'error'}>
              {scale.connected ? scale.name || 'Connected' : 'Not connected'}
            </Badge>
          }
        >
          <CardTitle icon={<Scale className="w-5 h-5" />}>Scale</CardTitle>
        </CardHeader>

        {scale.connected ? (
          <>
            <div className="grid grid-cols-2 gap-6 mb-6">
              <div className="text-center p-4 bg-theme-secondary rounded-xl">
                <Droplet className="w-6 h-6 text-accent mx-auto mb-2" />
                <div className="text-3xl font-bold text-theme tabular-nums">
                  {scale.weight.toFixed(1)}
                </div>
                <div className="text-sm text-theme-muted">grams</div>
              </div>
              <div className="text-center p-4 bg-theme-secondary rounded-xl">
                <Timer className="w-6 h-6 text-accent mx-auto mb-2" />
                <div className="text-3xl font-bold text-theme tabular-nums">
                  {scale.flowRate.toFixed(1)}
                </div>
                <div className="text-sm text-theme-muted">g/s flow</div>
              </div>
            </div>

            <div className="flex gap-3">
              <Button variant="secondary" onClick={tareScale}>
                Tare
              </Button>
              <Button variant="ghost" onClick={() => getConnection()?.sendCommand('scale_reset')}>
                Reset
              </Button>
            </div>
          </>
        ) : (
          <div className="text-center py-8">
            <Scale className="w-12 h-12 text-theme-muted mx-auto mb-3" />
            <p className="text-theme-muted">No scale connected</p>
            <Button variant="secondary" className="mt-4" onClick={() => window.location.href = '/scale'}>
              Go to Scale Settings
            </Button>
          </div>
        )}
      </Card>
    </div>
  );
}
