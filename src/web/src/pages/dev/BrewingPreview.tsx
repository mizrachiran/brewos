import { useState, useCallback } from 'react';
import { useStore } from '@/lib/store';
import { BrewingModeOverlay } from '@/components/BrewingModeOverlay';
import { Button } from '@/components/Button';
import { Card } from '@/components/Card';
import { Input } from '@/components/Input';
import { Toggle } from '@/components/Toggle';
import { useCommand } from '@/lib/useCommand';
import { Coffee, Play, Square, ArrowLeft, Send, Power } from 'lucide-react';
import { Link } from 'react-router-dom';

/**
 * Dev preview page for the Brewing Mode Overlay.
 * Allows testing the full-screen brewing UI without a real machine.
 * Access via /dev/brewing when dev mode is enabled (?dev=true)
 */
export function BrewingPreview() {
  const shot = useStore((s) => s.shot);
  const preinfusion = useStore((s) => s.preinfusion);
  const bbw = useStore((s) => s.bbw);
  const machineMode = useStore((s) => s.machine.mode);
  const machineState = useStore((s) => s.machine.state);
  const { sendCommand } = useCommand();

  // Local simulation controls (not synced to store until "Apply" is clicked)
  const [scaleConnected, setScaleConnected] = useState(true);
  const [enableBBW, setEnableBBW] = useState(true);
  const [targetWeight, setTargetWeight] = useState(36);
  const [simulatePreinfusion, setSimulatePreinfusion] = useState(true);
  const [preinfusionOnMs, setPreinfusionOnMs] = useState(3000);
  const [preinfusionPauseMs, setPreinfusionPauseMs] = useState(5000);

  const isMachineOn = machineMode === 'on';

  // Apply settings manually (not on every change)
  const applySettings = useCallback(() => {
    // Set scale connection status (for demo mode simulation)
    sendCommand('set_scale_connected', { connected: scaleConnected });
    
    sendCommand('set_preinfusion', {
      enabled: simulatePreinfusion,
      onTimeMs: preinfusionOnMs,
      pauseTimeMs: preinfusionPauseMs,
    });
    sendCommand('set_bbw', {
      enabled: enableBBW && scaleConnected, // BBW requires scale
      targetWeight,
      doseWeight: 18,
      stopOffset: 2,
      autoTare: true,
    });
  }, [scaleConnected, simulatePreinfusion, preinfusionOnMs, preinfusionPauseMs, enableBBW, targetWeight, sendCommand]);

  const turnMachineOn = useCallback(() => {
    sendCommand('set_mode', { mode: 'on', strategy: 1 });
  }, [sendCommand]);

  const startBrew = useCallback(() => {
    // Turn machine on first if needed, then start brewing
    if (!isMachineOn) {
      sendCommand('set_mode', { mode: 'on', strategy: 1 });
      // Give it a moment to turn on, then start brewing
      setTimeout(() => {
        sendCommand('brew_start');
      }, 100);
    } else {
      sendCommand('brew_start');
    }
  }, [sendCommand, isMachineOn]);

  const stopBrew = useCallback(() => {
    sendCommand('brew_stop');
  }, [sendCommand]);

  return (
    <div className="min-h-screen bg-theme p-6">
      <div className="max-w-2xl mx-auto space-y-6">
        {/* Header */}
        <div className="flex items-center gap-4">
          <Link
            to="/"
            className="p-2 rounded-lg hover:bg-theme-secondary transition-colors"
          >
            <ArrowLeft className="w-5 h-5 text-theme-secondary" />
          </Link>
          <div>
            <h1 className="text-2xl font-bold text-theme flex items-center gap-2">
              <Coffee className="w-6 h-6 text-accent" />
              Brewing Overlay Preview
            </h1>
            <p className="text-theme-muted text-sm">
              Dev mode - test the full-screen brewing experience
            </p>
          </div>
        </div>

        {/* Machine Status */}
        <Card>
          <div className="space-y-4">
            <div className="flex items-center justify-between">
              <div>
                <div className="text-sm text-theme-muted">Machine</div>
                <div className="text-lg font-semibold text-theme">
                  {isMachineOn ? (
                    <span className="text-emerald-500 flex items-center gap-2">
                      <span className="w-2 h-2 rounded-full bg-emerald-500" />
                      ON ({machineState})
                    </span>
                  ) : (
                    <span className="text-amber-500 flex items-center gap-2">
                      <span className="w-2 h-2 rounded-full bg-amber-500" />
                      Standby
                    </span>
                  )}
                </div>
              </div>
              {!isMachineOn && (
                <Button variant="secondary" onClick={turnMachineOn}>
                  <Power className="w-4 h-4 mr-2" />
                  Turn ON
                </Button>
              )}
            </div>

            <div className="flex items-center justify-between pt-4 border-t border-theme">
              <div>
                <div className="text-sm text-theme-muted">Brew State</div>
                <div className="text-lg font-semibold text-theme">
                  {shot.active ? (
                    <span className="text-emerald-500 flex items-center gap-2">
                      <span className="w-2 h-2 rounded-full bg-emerald-500 animate-pulse" />
                      Brewing in Progress
                    </span>
                  ) : (
                    <span className="text-theme-muted">Idle</span>
                  )}
                </div>
              </div>
              <div className="flex gap-2">
                {shot.active ? (
                  <Button variant="primary" onClick={stopBrew} className="bg-red-500 hover:bg-red-600">
                    <Square className="w-4 h-4 mr-2" />
                    Stop Brew
                  </Button>
                ) : (
                  <Button variant="primary" onClick={startBrew}>
                    <Play className="w-4 h-4 mr-2" />
                    Start Brew
                  </Button>
                )}
              </div>
            </div>

            {!isMachineOn && !shot.active && (
              <div className="text-xs text-amber-600 dark:text-amber-400 bg-amber-500/10 p-2 rounded-lg">
                💡 Machine will be turned ON automatically when you click Start Brew
              </div>
            )}
          </div>
        </Card>

        {/* Simulation Settings */}
        <Card>
          <h2 className="text-lg font-semibold text-theme mb-4">Simulation Settings</h2>
          
          <div className="space-y-4">
            {/* Scale Connection Toggle */}
            <div>
              <Toggle
                checked={scaleConnected}
                onChange={setScaleConnected}
                label="Scale Connected"
              />
              <p className="text-xs text-theme-muted mt-1">
                Simulate whether a Bluetooth scale is connected
              </p>
            </div>

            {/* Brew-by-Weight Toggle - only available when scale is connected */}
            <div className={!scaleConnected ? 'opacity-50' : ''}>
              <Toggle
                checked={enableBBW && scaleConnected}
                onChange={setEnableBBW}
                label="Enable Brew-by-Weight"
                disabled={!scaleConnected}
              />
              <p className="text-xs text-theme-muted mt-1">
                {scaleConnected 
                  ? 'When disabled, extraction is time-based only'
                  : 'Requires scale connection'}
              </p>
            </div>

            {/* Target Weight - only shown when BBW is enabled */}
            {enableBBW && (
              <div className="pl-4 border-l-2 border-accent/30">
                <label className="text-sm font-medium text-theme-secondary mb-1 block">
                  Target Weight (g)
                </label>
                <Input
                  type="number"
                  value={targetWeight}
                  onChange={(e) => setTargetWeight(Number(e.target.value))}
                  min={10}
                  max={100}
                />
                <p className="text-xs text-theme-muted mt-1">
                  Brew will auto-stop when this weight is reached
                </p>
              </div>
            )}

            {/* Pre-infusion Toggle */}
            <div className="pt-4 border-t border-theme">
              <Toggle
                checked={simulatePreinfusion}
                onChange={setSimulatePreinfusion}
                label="Enable Pre-infusion"
              />
            </div>

            {simulatePreinfusion && (
              <div className="grid grid-cols-2 gap-4 pl-4 border-l-2 border-accent/30">
                <div>
                  <label className="text-sm font-medium text-theme-secondary mb-1 block">
                    Pump ON Time (ms)
                  </label>
                  <Input
                    type="number"
                    value={preinfusionOnMs}
                    onChange={(e) => setPreinfusionOnMs(Number(e.target.value))}
                    min={500}
                    max={10000}
                    step={500}
                  />
                </div>
                <div>
                  <label className="text-sm font-medium text-theme-secondary mb-1 block">
                    Soak Time (ms)
                  </label>
                  <Input
                    type="number"
                    value={preinfusionPauseMs}
                    onChange={(e) => setPreinfusionPauseMs(Number(e.target.value))}
                    min={0}
                    max={30000}
                    step={1000}
                  />
                </div>
              </div>
            )}

            {/* Apply Button */}
            <div className="pt-4 border-t border-theme">
              <Button variant="secondary" onClick={applySettings} className="w-full">
                <Send className="w-4 h-4 mr-2" />
                Apply Settings
              </Button>
              <p className="text-xs text-theme-muted mt-2 text-center">
                Click to apply settings before starting brew
              </p>
            </div>
          </div>
        </Card>

        {/* Instructions */}
        <Card className="bg-amber-500/5 border-amber-500/20">
          <h3 className="text-sm font-semibold text-amber-700 dark:text-amber-300 mb-2">
            How to use
          </h3>
          <ul className="text-sm text-amber-800/80 dark:text-amber-200/80 space-y-1">
            <li>1. Configure your simulation settings above</li>
            <li>2. Click "Apply Settings" to sync them</li>
            <li>3. Click "Start Brew" to trigger the overlay</li>
            <li>4. The overlay will appear full-screen with simulated data</li>
            <li>5. Weight increases automatically, shot stops at target</li>
            <li>6. Click "Stop Brew" or use overlay controls to end early</li>
          </ul>
        </Card>

        {/* Current Settings Debug */}
        <details className="text-xs text-theme-muted">
          <summary className="cursor-pointer hover:text-theme-secondary">
            Debug: Current Store State
          </summary>
          <pre className="mt-2 p-3 bg-theme-secondary rounded-lg overflow-auto">
            {JSON.stringify({ shot, preinfusion, bbw, localSettings: { scaleConnected, enableBBW, targetWeight } }, null, 2)}
          </pre>
        </details>
      </div>

      {/* The actual overlay - appears when shot.active is true */}
      <BrewingModeOverlay />
    </div>
  );
}
