import { useStore } from "@/lib/store";
import { formatTime } from "@/lib/date";

interface LogViewerProps {
  maxHeight?: string;
}

function getLogColor(level: string): string {
  switch (level.toLowerCase()) {
    case "error":
      return "text-red-400";
    case "warn":
    case "warning":
      return "text-amber-400";
    case "info":
      return "text-blue-400";
    case "debug":
      return "text-theme-muted";
    default:
      return "text-theme-secondary";
  }
}

export function LogViewer({ maxHeight = "max-h-64" }: LogViewerProps) {
  const logs = useStore((s) => s.logs);

  return (
    <div
      className={`${maxHeight} overflow-y-auto bg-theme-secondary rounded-xl p-4 font-mono text-xs`}
    >
      {logs.length > 0 ? (
        logs.map((log) => (
          <div
            key={log.id}
            className="py-1 border-b border-theme last:border-0"
          >
            <span className="text-theme-muted">{formatTime(log.time)}</span>
            <span className={`ml-2 ${getLogColor(log.level)}`}>
              [{log.level.toUpperCase()}]
            </span>
            <span className="text-theme ml-2">{log.message}</span>
          </div>
        ))
      ) : (
        <p className="text-theme-muted text-center py-4">No logs yet</p>
      )}
    </div>
  );
}
