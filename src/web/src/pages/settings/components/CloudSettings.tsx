import React, { useState, useEffect } from 'react';
import { getConnection } from '@/lib/connection';
import { Card } from '@/components/Card';
import { Input } from '@/components/Input';
import { Button } from '@/components/Button';
import { Toggle } from '@/components/Toggle';
import { Badge } from '@/components/Badge';
import {
  QrCode,
  Copy,
  ExternalLink,
  Cloud,
  Wifi,
  Globe,
  Shield,
  RefreshCw,
  Check,
  X,
  Loader2,
} from 'lucide-react';
import { QRCodeSVG } from 'qrcode.react';

interface PairingData {
  deviceId: string;
  token: string;
  url: string;
  expiresIn: number;
}

interface CloudStatus {
  enabled: boolean;
  connected: boolean;
  serverUrl: string;
}

export function CloudSettings() {
  const [cloudConfig, setCloudConfig] = useState<CloudStatus | null>(null);
  const [pairing, setPairing] = useState<PairingData | null>(null);
  const [loadingQR, setLoadingQR] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);
  const [copiedCode, setCopiedCode] = useState(false);
  
  const [cloudEnabled, setCloudEnabled] = useState(cloudConfig?.enabled || false);
  const [cloudUrl, setCloudUrl] = useState(cloudConfig?.serverUrl || 'wss://cloud.brewos.io');
  const [saving, setSaving] = useState(false);

  const fetchPairingQR = async () => {
    setLoadingQR(true);
    setError(null);
    try {
      const response = await fetch('/api/pairing/qr');
      if (!response.ok) throw new Error('Failed to generate pairing code');
      const data = await response.json();
      setPairing(data);
    } catch {
      setError('Failed to generate pairing code. Make sure you\'re connected to your machine.');
    } finally {
      setLoadingQR(false);
    }
  };

  const refreshPairing = async () => {
    setLoadingQR(true);
    setError(null);
    try {
      const response = await fetch('/api/pairing/refresh', { method: 'POST' });
      if (!response.ok) throw new Error('Failed to refresh pairing code');
      const data = await response.json();
      setPairing(data);
    } catch {
      setError('Failed to refresh pairing code.');
    } finally {
      setLoadingQR(false);
    }
  };

  const copyPairingUrl = () => {
    if (pairing?.url) {
      navigator.clipboard.writeText(pairing.url);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const copyPairingCode = () => {
    if (pairing) {
      const code = `${pairing.deviceId}:${pairing.token}`;
      navigator.clipboard.writeText(code);
      setCopiedCode(true);
      setTimeout(() => setCopiedCode(false), 2000);
    }
  };

  const saveSettings = async () => {
    setSaving(true);
    try {
      getConnection()?.sendCommand('set_cloud_config', {
        enabled: cloudEnabled,
        serverUrl: cloudUrl,
      });
      setCloudConfig({
        enabled: cloudEnabled,
        connected: cloudConfig?.connected || false,
        serverUrl: cloudUrl,
      });
    } finally {
      setSaving(false);
    }
  };

  const fetchCloudStatus = async () => {
    try {
      const response = await fetch('/api/cloud/status');
      if (response.ok) {
        const data = await response.json();
        setCloudConfig(data);
        setCloudEnabled(data.enabled);
        setCloudUrl(data.serverUrl || 'wss://cloud.brewos.io');
      }
    } catch {
      // Device might not support cloud status endpoint yet
    }
  };

  useEffect(() => {
    fetchPairingQR();
    fetchCloudStatus();
  }, []);

  const isExpired = pairing?.expiresIn !== undefined && pairing.expiresIn <= 0;

  return (
    <div className="space-y-6">
      <div className="grid gap-6 md:grid-cols-2">
        {/* Pairing QR Code */}
        <Card>
          <div className="flex items-center gap-3 mb-4">
            <div className="p-2 bg-accent/10 rounded-lg">
              <QrCode className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-semibold text-coffee-900">Pair with Cloud</h2>
              <p className="text-sm text-coffee-500">Scan to add to your account</p>
            </div>
          </div>

          <div className="bg-white rounded-xl p-6 border border-cream-200 flex flex-col items-center">
            {loadingQR ? (
              <div className="w-48 h-48 flex items-center justify-center">
                <Loader2 className="w-8 h-8 animate-spin text-accent" />
              </div>
            ) : error ? (
              <div className="w-48 h-48 flex flex-col items-center justify-center text-center">
                <X className="w-8 h-8 text-red-500 mb-2" />
                <p className="text-sm text-coffee-500">{error}</p>
                <Button variant="secondary" size="sm" onClick={fetchPairingQR} className="mt-3">
                  Retry
                </Button>
              </div>
            ) : pairing ? (
              <>
                <div className={`p-3 bg-white rounded-xl ${isExpired ? 'opacity-50' : ''}`}>
                  <QRCodeSVG value={pairing.url} size={180} level="M" includeMargin={false} />
                </div>
                {isExpired ? (
                  <Badge variant="warning" className="mt-3">Code expired</Badge>
                ) : (
                  <p className="text-xs text-coffee-400 mt-2">
                    Expires in {Math.floor(pairing.expiresIn / 60)}m {pairing.expiresIn % 60}s
                  </p>
                )}
              </>
            ) : null}
          </div>

          <div className="mt-4 space-y-3">
            <div className="flex gap-2">
              <Button variant="secondary" className="flex-1" onClick={refreshPairing} disabled={loadingQR}>
                <RefreshCw className={`w-4 h-4 mr-2 ${loadingQR ? 'animate-spin' : ''}`} />
                New Code
              </Button>
              <Button variant="secondary" onClick={copyPairingUrl} disabled={!pairing}>
                {copied ? <Check className="w-4 h-4" /> : <Copy className="w-4 h-4" />}
              </Button>
            </div>
            
            {pairing && (
              <a 
                href={pairing.url}
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center justify-center gap-2 text-sm text-accent hover:underline"
              >
                Open pairing link
                <ExternalLink className="w-3 h-3" />
              </a>
            )}
          </div>

          {pairing && (
            <div className="mt-4 pt-4 border-t border-cream-200">
              <h3 className="text-sm font-medium text-coffee-900 mb-2">Manual Pairing Code</h3>
              <div className="flex items-center gap-2">
                <code className="flex-1 bg-cream-100 px-3 py-2 rounded-lg text-xs font-mono text-coffee-700 break-all">
                  {pairing.deviceId}:{pairing.token}
                </code>
                <Button variant="secondary" size="sm" onClick={copyPairingCode}>
                  {copiedCode ? <Check className="w-4 h-4" /> : <Copy className="w-4 h-4" />}
                </Button>
              </div>
              <p className="text-xs text-coffee-400 mt-2">
                Enter this code at cloud.brewos.io if you can't scan the QR
              </p>
            </div>
          )}
        </Card>

        {/* Cloud Status & Settings */}
        <div className="space-y-6">
          <Card>
            <div className="flex items-center gap-3 mb-4">
              <div className="p-2 bg-accent/10 rounded-lg">
                <Cloud className="w-5 h-5 text-accent" />
              </div>
              <div>
                <h2 className="font-semibold text-coffee-900">Cloud Status</h2>
                <p className="text-sm text-coffee-500">Connection to BrewOS Cloud</p>
              </div>
            </div>

            <div className="space-y-3">
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Wifi className="w-4 h-4 text-coffee-400" />
                  <span className="text-sm text-coffee-700">Cloud Connection</span>
                </div>
                <Badge variant={cloudConfig?.connected ? 'success' : 'default'}>
                  {cloudConfig?.connected ? 'Connected' : 'Disconnected'}
                </Badge>
              </div>
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Globe className="w-4 h-4 text-coffee-400" />
                  <span className="text-sm text-coffee-700">Server</span>
                </div>
                <span className="text-sm text-coffee-500 font-mono">
                  {cloudConfig?.serverUrl || 'Not configured'}
                </span>
              </div>
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Shield className="w-4 h-4 text-coffee-400" />
                  <span className="text-sm text-coffee-700">Device ID</span>
                </div>
                <span className="text-sm text-coffee-500 font-mono">
                  {pairing?.deviceId || '—'}
                </span>
              </div>
            </div>
          </Card>

          <Card>
            <div className="flex items-center gap-3 mb-4">
              <div className="p-2 bg-coffee-100 rounded-lg">
                <Cloud className="w-5 h-5 text-coffee-600" />
              </div>
              <div>
                <h2 className="font-semibold text-coffee-900">Cloud Settings</h2>
                <p className="text-sm text-coffee-500">Configure cloud connection</p>
              </div>
            </div>

            <div className="space-y-4">
              <div>
                <Toggle label="Enable Cloud Connection" checked={cloudEnabled} onChange={setCloudEnabled} />
                <p className="text-xs text-coffee-400 mt-1 ml-14">Allow remote access via BrewOS Cloud</p>
              </div>
              <Input
                label="Cloud Server URL"
                value={cloudUrl}
                onChange={(e) => setCloudUrl(e.target.value)}
                placeholder="wss://cloud.brewos.io"
                disabled={!cloudEnabled}
              />
              <Button onClick={saveSettings} loading={saving} disabled={!cloudEnabled && !cloudConfig?.enabled} className="w-full">
                Save Settings
              </Button>
            </div>
          </Card>
        </div>
      </div>
    </div>
  );
}

