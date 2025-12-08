import { useState, useRef, useEffect } from "react";
import { useStore } from "@/lib/store";
import { useCommand } from "@/lib/useCommand";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Leaf, Settings, ChevronRight } from "lucide-react";
import {
  convertFromCelsius,
  convertToCelsius,
  getUnitSymbol,
  getTemperatureStep,
  formatTemperatureWithUnit,
} from "@/lib/temperature";

// Default eco temperature in Celsius
const DEFAULT_ECO_TEMP_CELSIUS = 80;
const DEFAULT_ECO_TIMEOUT = 30;

export function EcoModeSettings() {
  const temperatureUnit = useStore((s) => s.preferences.temperatureUnit);
  const { sendCommand } = useCommand();

  const [savingEco, setSavingEco] = useState(false);
  const [editingEco, setEditingEco] = useState(false);

  // Store the actual value in Celsius (source of truth)
  const ecoTempCelsiusRef = useRef(DEFAULT_ECO_TEMP_CELSIUS);

  // Display value in current unit
  const [ecoBrewTempDisplay, setEcoBrewTempDisplay] = useState(() =>
    convertFromCelsius(DEFAULT_ECO_TEMP_CELSIUS, temperatureUnit)
  );
  const [ecoTimeout, setEcoTimeout] = useState(DEFAULT_ECO_TIMEOUT);

  // Track previous unit to detect changes
  const prevUnitRef = useRef(temperatureUnit);

  // Update display when unit changes (convert existing value to new unit)
  useEffect(() => {
    if (prevUnitRef.current !== temperatureUnit) {
      // Convert current display value to Celsius using the OLD unit
      const celsiusValue = convertToCelsius(
        ecoBrewTempDisplay,
        prevUnitRef.current
      );
      // Update the ref with the Celsius value
      ecoTempCelsiusRef.current = celsiusValue;
      // Convert Celsius to the NEW unit for display
      setEcoBrewTempDisplay(convertFromCelsius(celsiusValue, temperatureUnit));
      // Update the previous unit ref
      prevUnitRef.current = temperatureUnit;
    }
  }, [temperatureUnit, ecoBrewTempDisplay]);

  const unitSymbol = getUnitSymbol(temperatureUnit);
  const step = getTemperatureStep(temperatureUnit);

  // Calculate min/max in display unit
  const ecoTempMin = convertFromCelsius(60, temperatureUnit);
  const ecoTempMax = convertFromCelsius(90, temperatureUnit);

  // Get current Celsius value from display (for view mode)
  const currentTempCelsius = convertToCelsius(
    ecoBrewTempDisplay,
    temperatureUnit
  );

  const saveEco = () => {
    if (savingEco) return; // Prevent double-click
    setSavingEco(true);
    // Convert display value back to Celsius for backend
    const brewTempCelsius = convertToCelsius(
      ecoBrewTempDisplay,
      temperatureUnit
    );
    // Update the ref with the saved value
    ecoTempCelsiusRef.current = brewTempCelsius;
    sendCommand(
      "set_eco",
      { brewTemp: brewTempCelsius, timeout: ecoTimeout },
      { successMessage: "Eco settings saved" }
    );
    // Brief visual feedback for fire-and-forget WebSocket command
    setTimeout(() => setSavingEco(false), 600);
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Leaf className="w-5 h-5" />}>Eco Mode</CardTitle>
      </CardHeader>

      {!editingEco ? (
        /* View mode */
        <div className="space-y-0">
          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Eco Temperature</span>
            <span className="text-sm font-medium text-theme">
              {formatTemperatureWithUnit(currentTempCelsius, temperatureUnit, 0)}
            </span>
          </div>
          <div className="flex items-center justify-between py-2 border-b border-theme">
            <span className="text-sm text-theme-muted">Auto-Eco After</span>
            <span className="text-sm font-medium text-theme">
              {ecoTimeout} min
            </span>
          </div>

          <button
            onClick={() => setEditingEco(true)}
            className="w-full flex items-center justify-between py-2.5 border-t border-theme text-left group transition-colors hover:opacity-80 mt-2"
          >
            <div className="flex items-center gap-3">
              <Settings className="w-4 h-4 text-theme-muted" />
              <span className="text-sm font-medium text-theme">
                Configure Eco Mode
              </span>
            </div>
            <ChevronRight className="w-4 h-4 text-theme-muted group-hover:text-theme transition-colors" />
          </button>
        </div>
      ) : (
        /* Edit mode */
        <div className="space-y-4">
          <p className="text-sm text-theme-muted">
            Reduce power consumption when idle by lowering boiler temperatures.
          </p>

          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <Input
              label="Eco Brew Temp"
              type="number"
              min={ecoTempMin}
              max={ecoTempMax}
              step={step}
              value={ecoBrewTempDisplay}
              onChange={(e) =>
                setEcoBrewTempDisplay(parseFloat(e.target.value))
              }
              unit={unitSymbol}
            />
            <Input
              label="Auto-Eco After"
              type="number"
              min={5}
              max={120}
              step={5}
              value={ecoTimeout}
              onChange={(e) => setEcoTimeout(parseInt(e.target.value))}
              unit="min"
            />
          </div>

          <div className="flex justify-end gap-3">
            <Button variant="ghost" onClick={() => setEditingEco(false)}>
              Cancel
            </Button>
            <Button
              onClick={() => {
                saveEco();
                setEditingEco(false);
              }}
              loading={savingEco}
              disabled={savingEco}
            >
              Save
            </Button>
          </div>
        </div>
      )}
    </Card>
  );
}
