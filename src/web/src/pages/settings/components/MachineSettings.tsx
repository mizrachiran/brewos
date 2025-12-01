import React, { useState, useEffect } from 'react';
import { useStore } from '@/lib/store';
import { getConnection } from '@/lib/connection';
import { Card, CardHeader, CardTitle } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Coffee, Save } from 'lucide-react';
import { StatusRow } from './StatusRow';

export function MachineSettings() {
  const device = useStore((s) => s.device);

  // Device identity
  const [deviceName, setDeviceName] = useState(device.deviceName);
  const [machineBrand, setMachineBrand] = useState(device.machineBrand || '');
  const [machineModel, setMachineModel] = useState(device.machineModel || '');
  const [savingMachine, setSavingMachine] = useState(false);
  
  // Sync device info when it changes from server
  useEffect(() => {
    setDeviceName(device.deviceName);
    setMachineBrand(device.machineBrand || '');
    setMachineModel(device.machineModel || '');
  }, [device.deviceName, device.machineBrand, device.machineModel]);

  const saveMachineInfo = async () => {
    if (!deviceName.trim() || !machineBrand.trim() || !machineModel.trim()) return;
    setSavingMachine(true);
    getConnection()?.sendCommand('set_machine_info', { 
      name: deviceName.trim(),
      brand: machineBrand.trim(),
      model: machineModel.trim(),
    });
    // Simulate success
    setTimeout(() => setSavingMachine(false), 500);
  };
  
  const isMachineInfoValid = deviceName.trim() && machineBrand.trim() && machineModel.trim();
  const isMachineInfoChanged = 
    deviceName !== device.deviceName || 
    machineBrand !== (device.machineBrand || '') || 
    machineModel !== (device.machineModel || '');

  return (
    <Card>
      <CardHeader>
        <CardTitle icon={<Coffee className="w-5 h-5" />}>
          Machine
        </CardTitle>
      </CardHeader>

      <div className="space-y-4">
        <Input
          label="Machine Name"
          placeholder="Kitchen Espresso"
          value={deviceName}
          onChange={(e) => setDeviceName(e.target.value)}
          hint="Give your machine a friendly name"
          required
        />
        
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
          <Input
            label="Brand"
            placeholder="ECM, Profitec, Rancilio..."
            value={machineBrand}
            onChange={(e) => setMachineBrand(e.target.value)}
            hint="Machine manufacturer"
            required
          />
          <Input
            label="Model"
            placeholder="Synchronika, Pro 700, Silvia..."
            value={machineModel}
            onChange={(e) => setMachineModel(e.target.value)}
            hint="Machine model name"
            required
          />
        </div>
        
        <div className="flex justify-end">
          <Button 
            onClick={saveMachineInfo} 
            loading={savingMachine}
            disabled={!isMachineInfoValid || !isMachineInfoChanged}
          >
            <Save className="w-4 h-4" />
            Save Machine Info
          </Button>
        </div>
        
        {device.deviceId && (
          <div className="grid grid-cols-2 gap-4 pt-2 border-t border-cream-200">
            <StatusRow label="Device ID" value={device.deviceId} mono />
            <StatusRow label="Machine Type" value={device.machineType || 'Not set'} />
          </div>
        )}
      </div>
    </Card>
  );
}

