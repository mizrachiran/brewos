import { useState } from "react";
import { FileText, AlertCircle, Info, AlertTriangle, Bug } from "lucide-react";

// Note: In a real implementation, logs would come from an API endpoint
// For now, this is a placeholder showing the UI structure

interface LogEntry {
  id: string;
  timestamp: string;
  level: "info" | "warn" | "error" | "debug";
  message: string;
  source?: string;
}

const MOCK_LOGS: LogEntry[] = [
  {
    id: "1",
    timestamp: new Date().toISOString(),
    level: "info",
    message: "[Auth] User logged in via Google",
    source: "auth",
  },
  {
    id: "2",
    timestamp: new Date(Date.now() - 60000).toISOString(),
    level: "info",
    message: "[Device] Connected: BRW-12345678 (authenticated)",
    source: "device",
  },
  {
    id: "3",
    timestamp: new Date(Date.now() - 120000).toISOString(),
    level: "warn",
    message: "[Device] BRW-12345678 ping timeout (2 missed) - disconnecting",
    source: "device",
  },
  {
    id: "4",
    timestamp: new Date(Date.now() - 180000).toISOString(),
    level: "error",
    message: "[Cleanup] Error during orphaned device cleanup",
    source: "cleanup",
  },
];

export function Logs() {
  const [logs] = useState<LogEntry[]>(MOCK_LOGS);
  const [levelFilter, setLevelFilter] = useState<string>("all");

  const filteredLogs =
    levelFilter === "all"
      ? logs
      : logs.filter((log) => log.level === levelFilter);

  function getLevelIcon(level: string) {
    switch (level) {
      case "error":
        return <AlertCircle className="w-4 h-4 text-admin-danger" />;
      case "warn":
        return <AlertTriangle className="w-4 h-4 text-admin-warning" />;
      case "debug":
        return <Bug className="w-4 h-4 text-admin-text-secondary" />;
      default:
        return <Info className="w-4 h-4 text-admin-accent" />;
    }
  }

  function getLevelClass(level: string) {
    switch (level) {
      case "error":
        return "text-admin-danger";
      case "warn":
        return "text-admin-warning";
      case "debug":
        return "text-admin-text-secondary";
      default:
        return "text-admin-accent";
    }
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
        <div>
          <h1 className="text-xl sm:text-2xl font-display font-bold text-admin-text flex items-center gap-3">
            <FileText className="w-6 h-6 sm:w-7 sm:h-7 text-admin-accent" />
            Logs
          </h1>
          <p className="text-admin-text-secondary mt-1 text-sm sm:text-base">
            Server logs and activity
          </p>
        </div>
        <select
          value={levelFilter}
          onChange={(e) => setLevelFilter(e.target.value)}
          className="admin-input w-full sm:w-auto"
        >
          <option value="all">All Levels</option>
          <option value="info">Info</option>
          <option value="warn">Warnings</option>
          <option value="error">Errors</option>
          <option value="debug">Debug</option>
        </select>
      </div>

      {/* Logs */}
      <div className="admin-card">
        <div className="space-y-1 font-mono text-sm">
          {filteredLogs.length === 0 ? (
            <p className="text-admin-text-secondary text-center py-8">
              No logs to display
            </p>
          ) : (
            filteredLogs.map((log) => (
              <div
                key={log.id}
                className="flex items-start gap-3 py-2 px-3 hover:bg-admin-hover rounded-lg transition-colors"
              >
                <span className="text-admin-text-secondary text-xs whitespace-nowrap">
                  {new Date(log.timestamp).toLocaleTimeString()}
                </span>
                {getLevelIcon(log.level)}
                <span className={`flex-1 ${getLevelClass(log.level)}`}>
                  {log.message}
                </span>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Note */}
      <div className="bg-admin-surface border border-admin-border rounded-lg p-4">
        <p className="text-admin-text-secondary text-sm">
          <strong>Note:</strong> This is a placeholder. In production, logs would
          be streamed from the server via WebSocket or fetched from a logging
          service like Loki, CloudWatch, or similar.
        </p>
      </div>
    </div>
  );
}

