import { useState } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Zap, Leaf } from 'lucide-react';

export function PowerSettings() {
  const power = useStore((s) => s.power);

  const [voltage, setVoltage] = useState(power.voltage);
  const [maxCurrent, setMaxCurrent] = useState(13);
  const [ecoSettings, setEcoSettings] = useState({
    brewTemp: 80,
    timeout: 30,
  });

  const savePower = () => {
    getConnection()?.sendCommand('set_power', { voltage, maxCurrent });
  };

  const saveEco = () => {
    getConnection()?.sendCommand('set_eco', ecoSettings);
  };

  return (
    <>
      {/* Power Settings */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Zap className="w-5 h-5" />}>Power</CardTitle>
        </CardHeader>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
          <div className="space-y-1.5">
            <label className="block text-xs font-semibold uppercase tracking-wider text-coffee-500">
              Mains Voltage
            </label>
            <select
              value={voltage}
              onChange={(e) => setVoltage(parseInt(e.target.value))}
              className="input"
            >
              <option value="110">110V (US)</option>
              <option value="220">220V (EU/AU)</option>
              <option value="240">240V (UK)</option>
            </select>
          </div>
          <Input
            label="Max Current"
            type="number"
            min={5}
            max={20}
            step={0.5}
            value={maxCurrent}
            onChange={(e) => setMaxCurrent(parseFloat(e.target.value))}
            unit="A"
            hint="Limit for your circuit"
          />
        </div>

        <Button onClick={savePower}>Save Power Settings</Button>
      </Card>

      {/* Eco Mode */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Leaf className="w-5 h-5" />}>Eco Mode</CardTitle>
        </CardHeader>

        <p className="text-sm text-coffee-500 mb-4">
          Reduce power consumption when idle by lowering boiler temperatures.
        </p>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
          <Input
            label="Eco Brew Temp"
            type="number"
            min={60}
            max={90}
            value={ecoSettings.brewTemp}
            onChange={(e) => setEcoSettings({ ...ecoSettings, brewTemp: parseFloat(e.target.value) })}
            unit="°C"
          />
          <Input
            label="Auto-Eco After"
            type="number"
            min={5}
            max={120}
            step={5}
            value={ecoSettings.timeout}
            onChange={(e) => setEcoSettings({ ...ecoSettings, timeout: parseInt(e.target.value) })}
            unit="min"
          />
        </div>

        <Button onClick={saveEco}>Save Eco Settings</Button>
      </Card>
    </>
  );
}

