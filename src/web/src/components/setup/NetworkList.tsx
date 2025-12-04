import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Wifi, Loader2, RefreshCw } from "lucide-react";

export interface Network {
  ssid: string;
  rssi: number;
  secure: boolean;
}

interface NetworkListProps {
  networks: Network[];
  selectedSsid?: string;
  password?: string;
  scanning?: boolean;
  error?: string;
  onNetworkSelect?: (ssid: string) => void;
  onPasswordChange?: (password: string) => void;
  onRefresh?: () => void;
  onConnect?: () => void;
}

export function NetworkList({
  networks,
  selectedSsid,
  password,
  scanning = false,
  error,
  onNetworkSelect,
  onPasswordChange,
  onRefresh,
  onConnect,
}: NetworkListProps) {
  return (
    <>
      {/* Network List */}
      <div className="mb-4">
        <div className="flex items-center justify-between mb-2">
          <label className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
            Available Networks
          </label>
          <Button variant="ghost" size="sm" disabled={scanning} onClick={onRefresh}>
            {scanning ? (
              <Loader2 className="w-4 h-4 animate-spin" />
            ) : (
              <RefreshCw className="w-4 h-4" />
            )}
          </Button>
        </div>

        <div className="max-h-48 overflow-y-auto border border-theme rounded-xl">
          {networks.length === 0 ? (
            <div className="p-4 text-center text-theme-muted">
              {scanning ? "Scanning..." : "No networks found"}
            </div>
          ) : (
            networks.map((network) => (
              <button
                key={network.ssid}
                onClick={() => onNetworkSelect?.(network.ssid)}
                className={`w-full flex items-center justify-between p-3 border-b border-theme last:border-0 hover:bg-theme-secondary transition-colors ${
                  selectedSsid === network.ssid ? "bg-accent/10" : ""
                }`}
              >
                <div className="flex items-center gap-3">
                  <Wifi
                    className={`w-4 h-4 ${
                      network.rssi > -50
                        ? "text-emerald-500"
                        : network.rssi > -70
                        ? "text-amber-500"
                        : "text-red-500"
                    }`}
                  />
                  <span className="font-medium text-theme">{network.ssid}</span>
                </div>
                {network.secure && (
                  <span className="text-xs text-theme-muted">🔒</span>
                )}
              </button>
            ))
          )}
        </div>
      </div>

      {/* Password */}
      {selectedSsid && (
        <div className="mb-6">
          <Input
            label="Password"
            type="password"
            placeholder="Enter WiFi password"
            value={password}
            onChange={(e) => onPasswordChange?.(e.target.value)}
          />
        </div>
      )}

      {/* Error */}
      {error && (
        <div className="mb-4 p-3 bg-error-soft text-error rounded-xl text-sm">
          {error}
        </div>
      )}

      {/* Connect Button */}
      <Button className="w-full" disabled={!selectedSsid} onClick={onConnect}>
        Connect
      </Button>
    </>
  );
}

