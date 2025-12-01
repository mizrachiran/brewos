import { useState } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import { Wifi, Radio, X, Check } from 'lucide-react';
import { StatusRow } from './StatusRow';

export function NetworkSettings() {
  const wifi = useStore((s) => s.wifi);
  const mqtt = useStore((s) => s.mqtt);

  const [mqttConfig, setMqttConfig] = useState({
    enabled: mqtt.enabled,
    broker: mqtt.broker || '',
    port: 1883,
    username: '',
    password: '',
    topic: mqtt.topic || 'brewos',
    discovery: true,
  });
  const [testingMqtt, setTestingMqtt] = useState(false);

  const testMqtt = async () => {
    setTestingMqtt(true);
    getConnection()?.sendCommand('mqtt_test', mqttConfig);
    setTimeout(() => setTestingMqtt(false), 3000);
  };

  const saveMqtt = () => {
    getConnection()?.sendCommand('mqtt_config', mqttConfig);
  };

  const forgetWifi = () => {
    if (confirm('Are you sure? The device will restart in AP mode.')) {
      getConnection()?.sendCommand('wifi_forget');
    }
  };

  return (
    <>
      {/* WiFi Status */}
      <Card>
        <CardHeader>
          <CardTitle icon={<Wifi className="w-5 h-5" />}>WiFi</CardTitle>
        </CardHeader>

        <div className="space-y-3 mb-6">
          <StatusRow 
            label="Status" 
            value={
              <Badge variant={wifi.connected ? 'success' : wifi.apMode ? 'warning' : 'error'}>
                {wifi.connected ? 'Connected' : wifi.apMode ? 'AP Mode' : 'Disconnected'}
              </Badge>
            }
          />
          <StatusRow label="Network" value={wifi.ssid || '—'} />
          <StatusRow label="IP Address" value={wifi.ip || '—'} mono />
          <StatusRow 
            label="Signal" 
            value={wifi.rssi ? `${wifi.rssi} dBm` : '—'} 
          />
        </div>

        <Button variant="ghost" onClick={forgetWifi}>
          <X className="w-4 h-4" />
          Forget Network
        </Button>
      </Card>

      {/* MQTT Settings */}
      <Card>
        <CardHeader
          action={
            <Toggle
              checked={mqttConfig.enabled}
              onChange={(enabled) => setMqttConfig({ ...mqttConfig, enabled })}
            />
          }
        >
          <CardTitle icon={<Radio className="w-5 h-5" />}>
            MQTT / Home Assistant
          </CardTitle>
        </CardHeader>

        <div className="space-y-4 mb-6">
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <Input
              label="Broker Address"
              placeholder="homeassistant.local"
              value={mqttConfig.broker}
              onChange={(e) => setMqttConfig({ ...mqttConfig, broker: e.target.value })}
            />
            <Input
              label="Topic Prefix"
              placeholder="brewos"
              value={mqttConfig.topic}
              onChange={(e) => setMqttConfig({ ...mqttConfig, topic: e.target.value })}
              hint="Topics: {prefix}/status, {prefix}/command"
            />
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
            <Input
              label="Port"
              type="number"
              value={mqttConfig.port}
              onChange={(e) => setMqttConfig({ ...mqttConfig, port: parseInt(e.target.value) })}
            />
            <Input
              label="Username"
              placeholder="Optional"
              value={mqttConfig.username}
              onChange={(e) => setMqttConfig({ ...mqttConfig, username: e.target.value })}
            />
            <Input
              label="Password"
              type="password"
              placeholder="Optional"
              value={mqttConfig.password}
              onChange={(e) => setMqttConfig({ ...mqttConfig, password: e.target.value })}
            />
          </div>

          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={mqttConfig.discovery}
              onChange={(e) => setMqttConfig({ ...mqttConfig, discovery: e.target.checked })}
              className="w-4 h-4 rounded border-cream-300 text-accent focus:ring-accent"
            />
            <span className="text-sm text-coffee-700">Enable Home Assistant auto-discovery</span>
          </label>

          <div className="flex items-center gap-2 p-3 bg-cream-100 rounded-xl">
            {mqtt.connected ? (
              <Check className="w-4 h-4 text-emerald-600" />
            ) : mqtt.enabled ? (
              <X className="w-4 h-4 text-red-500" />
            ) : (
              <div className="w-4 h-4 rounded-full bg-cream-300" />
            )}
            <span className="text-sm text-coffee-700">
              {mqtt.connected ? 'Connected to broker' : mqtt.enabled ? 'Disconnected' : 'Disabled'}
            </span>
          </div>
        </div>

        <div className="flex gap-3">
          <Button variant="secondary" onClick={testMqtt} loading={testingMqtt}>
            {testingMqtt ? 'Testing...' : 'Test Connection'}
          </Button>
          <Button onClick={saveMqtt}>Save MQTT Settings</Button>
        </div>
      </Card>
    </>
  );
}

