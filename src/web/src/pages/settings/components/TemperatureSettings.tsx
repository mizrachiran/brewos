import { useState, useEffect, useMemo } from 'react';
import { useStore } from '@/lib/store';
import { useCommand } from '@/lib/useCommand';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Thermometer, Coffee, Wind, ChevronRight, Settings } from 'lucide-react';
import { 
  convertFromCelsius, 
  convertToCelsius, 
  getUnitSymbol, 
  getTemperatureStep,
  getTemperatureRanges,
  formatTemperatureWithUnit,
} from '@/lib/temperature';

export function TemperatureSettings() {
  const temps = useStore((s) => s.temps);
  const device = useStore((s) => s.device);
  const temperatureUnit = useStore((s) => s.preferences.temperatureUnit);
  const { sendCommand } = useCommand();
  const [saving, setSaving] = useState(false);
  const [editing, setEditing] = useState(false);

  // Store values in display unit for the UI
  const [brewTempDisplay, setBrewTempDisplay] = useState(() => 
    convertFromCelsius(temps.brew.setpoint, temperatureUnit)
  );
  const [steamTempDisplay, setSteamTempDisplay] = useState(() => 
    convertFromCelsius(temps.steam.setpoint, temperatureUnit)
  );

  // Get temperature ranges in current unit
  const ranges = useMemo(() => getTemperatureRanges(temperatureUnit), [temperatureUnit]);
  const step = getTemperatureStep(temperatureUnit);
  const unitSymbol = getUnitSymbol(temperatureUnit);

  // Update display values when unit changes or temps change from server
  // Only update if not editing to prevent overwriting user input
  useEffect(() => {
    if (!editing) {
      setBrewTempDisplay(convertFromCelsius(temps.brew.setpoint, temperatureUnit));
      setSteamTempDisplay(convertFromCelsius(temps.steam.setpoint, temperatureUnit));
    }
  }, [temps.brew.setpoint, temps.steam.setpoint, temperatureUnit, editing]);

  const isDualBoiler = device.machineType === 'dual_boiler';
  const isSingleBoiler = device.machineType === 'single_boiler';
  const isHeatExchanger = device.machineType === 'heat_exchanger';

  const saveTemps = () => {
    if (saving) return; // Prevent double-click
    setSaving(true);
    
    // Convert back to Celsius before sending to backend
    const brewTempCelsius = convertToCelsius(brewTempDisplay, temperatureUnit);
    const steamTempCelsius = convertToCelsius(steamTempDisplay, temperatureUnit);

    let success = true;
    if (isDualBoiler || isSingleBoiler || !device.machineType) {
      success = sendCommand('set_temp', { boiler: 'brew', temp: brewTempCelsius });
    }
    if (success && (isDualBoiler || isHeatExchanger || !device.machineType)) {
      sendCommand('set_temp', { boiler: 'steam', temp: steamTempCelsius }, 
        { successMessage: 'Temperatures saved' });
    }
    
    // Optimistically update the store to prevent UI flicker
    // This ensures the "View Mode" shows the new values immediately when editing closes
    // even if the machine status update hasn't arrived yet
    // Use timestamp-based conflict resolution to prevent race conditions
    const optimisticTimestamp = Date.now();
    useStore.setState((state) => {
      // Only apply the optimistic update if no newer data has been received
      if (!state.temps.lastUpdated || state.temps.lastUpdated <= optimisticTimestamp) {
        return {
          temps: {
            ...state.temps,
            brew: {
              ...state.temps.brew,
              setpoint: brewTempCelsius
            },
            steam: {
              ...state.temps.steam,
              setpoint: steamTempCelsius
            },
            lastUpdated: optimisticTimestamp,
          }
        };
      }
      // If newer data has already arrived, do not apply the optimistic update
      return state;
    });
    
    // Brief visual feedback for fire-and-forget WebSocket command
    setTimeout(() => setSaving(false), 600);
  };

  // Determine what controls to show based on machine type
  const showBrewControl = isDualBoiler || isSingleBoiler || !device.machineType;
  const showSteamControl = isDualBoiler || isHeatExchanger || !device.machineType;

  // Get appropriate labels based on machine type
  const brewLabel = isSingleBoiler ? 'Boiler' : 'Brew';
  const steamLabel = isHeatExchanger ? 'Boiler' : 'Steam';

  // Get hints for edit mode
  const brewHint = isSingleBoiler 
    ? `Brew mode: ${ranges.brew.recommended.min.toFixed(0)}-${ranges.brew.recommended.max.toFixed(0)}${unitSymbol}`
    : `Recommended: ${ranges.brew.recommended.min.toFixed(0)}-${ranges.brew.recommended.max.toFixed(0)}${unitSymbol}`;
  const steamHint = isHeatExchanger 
    ? 'Controls HX brew water temperature' 
    : 'For milk frothing';

  // Calculate min/max for inputs
  const brewMin = ranges.brew.min;
  const brewMax = isSingleBoiler ? ranges.boiler.max : ranges.brew.max;
  const steamMin = isHeatExchanger ? convertFromCelsius(100, temperatureUnit) : ranges.steam.min;
  const steamMax = ranges.steam.max;

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Thermometer className="w-5 h-5" />}>
          Temperature
        </CardTitle>
      </CardHeader>

      {!editing ? (
        /* View mode */
        <div className="space-y-4">
          {/* Temperature cards */}
          <div className={`grid gap-3 ${showBrewControl && showSteamControl ? 'grid-cols-2' : 'grid-cols-1'}`}>
            {showBrewControl && (
              <div className="p-4 rounded-xl bg-gradient-to-br from-amber-500/10 to-orange-500/5 border border-amber-500/20">
                <div className="flex items-center gap-2 mb-2">
                  <Coffee className="w-4 h-4 text-amber-500" />
                  <span className="text-xs font-medium text-amber-500 uppercase tracking-wide">
                    {brewLabel}
                  </span>
                </div>
                <div className="text-2xl font-bold text-theme tabular-nums">
                  {formatTemperatureWithUnit(temps.brew.setpoint, temperatureUnit, 1)}
                </div>
                <div className="text-xs text-theme-muted mt-1">
                  Target setpoint
                </div>
              </div>
            )}
            {showSteamControl && (
              <div className="p-4 rounded-xl bg-gradient-to-br from-blue-500/10 to-cyan-500/5 border border-blue-500/20">
                <div className="flex items-center gap-2 mb-2">
                  <Wind className="w-4 h-4 text-blue-500" />
                  <span className="text-xs font-medium text-blue-500 uppercase tracking-wide">
                    {steamLabel}
                  </span>
                </div>
                <div className="text-2xl font-bold text-theme tabular-nums">
                  {formatTemperatureWithUnit(temps.steam.setpoint, temperatureUnit, 1)}
                </div>
                <div className="text-xs text-theme-muted mt-1">
                  Target setpoint
                </div>
              </div>
            )}
          </div>

          {/* Configure button */}
          <button
            onClick={() => setEditing(true)}
            className="w-full flex items-center justify-between py-2.5 border-t border-theme text-left group transition-colors hover:opacity-80"
          >
            <div className="flex items-center gap-3">
              <Settings className="w-4 h-4 text-theme-muted" />
              <span className="text-sm font-medium text-theme">Adjust Temperatures</span>
            </div>
            <ChevronRight className="w-4 h-4 text-theme-muted group-hover:text-theme transition-colors" />
          </button>
        </div>
      ) : (
        /* Edit mode */
        <div className="space-y-4">
          <div className={`grid grid-cols-1 ${showBrewControl && showSteamControl ? 'sm:grid-cols-2' : ''} gap-4`}>
            {showBrewControl && (
              <Input
                label={brewLabel + ' Temperature'}
                type="number"
                min={brewMin}
                max={brewMax}
                step={step}
                value={brewTempDisplay}
                onChange={(e) => setBrewTempDisplay(parseFloat(e.target.value))}
                unit={unitSymbol}
                hint={brewHint}
              />
            )}
            {showSteamControl && (
              <Input
                label={steamLabel + ' Temperature'}
                type="number"
                min={steamMin}
                max={steamMax}
                step={step}
                value={steamTempDisplay}
                onChange={(e) => setSteamTempDisplay(parseFloat(e.target.value))}
                unit={unitSymbol}
                hint={steamHint}
              />
            )}
          </div>

          <div className="flex justify-end gap-3">
            <Button variant="ghost" onClick={() => setEditing(false)}>
              Cancel
            </Button>
            <Button 
              onClick={() => {
                saveTemps();
                setEditing(false);
              }} 
              loading={saving} 
              disabled={saving}
            >
              Save
            </Button>
          </div>
        </div>
      )}
    </Card>
  );
}

