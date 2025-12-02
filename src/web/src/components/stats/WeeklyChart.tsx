export interface WeeklyData {
  day: string;
  shots: number;
}

export interface WeeklyChartProps {
  data: WeeklyData[];
  emptyMessage?: string;
}

export function WeeklyChart({ data, emptyMessage = "No data available yet" }: WeeklyChartProps) {
  const maxShots = data.length > 0 ? Math.max(...data.map((d) => d.shots), 1) : 5;

  if (data.length === 0) {
    return (
      <div className="flex items-center justify-center h-full text-theme-muted">
        <p>{emptyMessage}</p>
      </div>
    );
  }

  return (
    <div className="flex items-end justify-between h-full gap-2 pt-4">
      {data.map((item) => (
        <div key={item.day} className="flex-1 flex flex-col items-center">
          <div className="w-full flex flex-col items-center justify-end h-32">
            <span className="text-xs font-semibold text-theme-secondary mb-1">
              {item.shots > 0 ? item.shots : ""}
            </span>
            <div
              className="w-full max-w-12 bg-gradient-to-t from-accent to-accent-light rounded-t-lg transition-all duration-500"
              style={{
                height: `${Math.max((item.shots / maxShots) * 100, 4)}%`,
                opacity: item.shots > 0 ? 1 : 0.2,
              }}
            />
          </div>
          <span className="text-xs text-theme-muted mt-2 font-medium">
            {item.day}
          </span>
        </div>
      ))}
    </div>
  );
}

