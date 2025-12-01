import { useState } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Thermometer } from 'lucide-react';

export function TemperatureSettings() {
  const temps = useStore((s) => s.temps);

  const [brewTemp, setBrewTemp] = useState(temps.brew.setpoint);
  const [steamTemp, setSteamTemp] = useState(temps.steam.setpoint);

  const saveTemps = () => {
    getConnection()?.sendCommand('set_temp', { boiler: 'brew', temp: brewTemp });
    getConnection()?.sendCommand('set_temp', { boiler: 'steam', temp: steamTemp });
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Thermometer className="w-5 h-5" />}>
          Temperature
        </CardTitle>
      </CardHeader>

      <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-6">
        <Input
          label="Brew Temperature"
          type="number"
          min={80}
          max={105}
          step={0.5}
          value={brewTemp}
          onChange={(e) => setBrewTemp(parseFloat(e.target.value))}
          unit="°C"
          hint="Recommended: 92-96°C"
        />
        <Input
          label="Steam Temperature"
          type="number"
          min={120}
          max={160}
          step={1}
          value={steamTemp}
          onChange={(e) => setSteamTemp(parseFloat(e.target.value))}
          unit="°C"
          hint="For milk frothing"
        />
      </div>

      <Button onClick={saveTemps}>Save Temperatures</Button>
    </Card>
  );
}

