import { memo, useState, useEffect, useRef, useCallback } from "react";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Gauge as GaugeIcon } from "lucide-react";
import { useStore } from "@/lib/store";

interface PressureCardProps {
  pressure: number;
}

interface DataPoint {
  time: number; // seconds since start
  pressure: number;
}

const MAX_PRESSURE = 12; // bar - typical max for espresso
const GRAPH_DURATION = 45; // seconds to show on x-axis

export const PressureCard = memo(function PressureCard({ pressure }: PressureCardProps) {
  const isBrewing = useStore((s) => s.machine.isBrewing);
  const shotActive = useStore((s) => s.shot.active);
  const shotStartTime = useStore((s) => s.shot.startTime);
  
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [showGraph, setShowGraph] = useState(false);
  const lastPressureRef = useRef(pressure);
  const animationFrameRef = useRef<number>();
  
  // Track if we're currently brewing (either flag)
  const isCurrentlyBrewing = isBrewing || shotActive;
  
  // Start tracking when brewing begins
  useEffect(() => {
    if (isCurrentlyBrewing && !showGraph) {
      setShowGraph(true);
      setDataPoints([]);
    }
  }, [isCurrentlyBrewing, showGraph]);
  
  // Record pressure data points during brewing
  useEffect(() => {
    if (!isCurrentlyBrewing || !showGraph) return;
    
    const startTime = shotStartTime || Date.now();
    
    const recordPoint = () => {
      const elapsed = (Date.now() - startTime) / 1000;
      
      setDataPoints(prev => {
        // Only add if pressure changed or it's the first point
        const lastPoint = prev[prev.length - 1];
        const roundedPressure = Math.round(lastPressureRef.current * 10) / 10;
        
        if (!lastPoint || Math.abs(lastPoint.pressure - roundedPressure) > 0.05 || elapsed - lastPoint.time > 0.5) {
          return [...prev, { time: elapsed, pressure: roundedPressure }];
        }
        return prev;
      });
      
      if (isCurrentlyBrewing) {
        animationFrameRef.current = requestAnimationFrame(() => {
          setTimeout(recordPoint, 100); // Record every 100ms
        });
      }
    };
    
    recordPoint();
    
    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [isCurrentlyBrewing, showGraph, shotStartTime]);
  
  // Update ref when pressure changes
  useEffect(() => {
    lastPressureRef.current = pressure;
  }, [pressure]);
  
  // Keep graph visible for a few seconds after brewing ends
  useEffect(() => {
    if (!isCurrentlyBrewing && showGraph && dataPoints.length > 0) {
      const timer = setTimeout(() => {
        // Don't hide - keep showing last shot's graph
      }, 3000);
      return () => clearTimeout(timer);
    }
  }, [isCurrentlyBrewing, showGraph, dataPoints.length]);
  
  const clearGraph = useCallback(() => {
    setDataPoints([]);
    setShowGraph(false);
  }, []);

  return (
    <Card className="relative">
      <CardHeader
        action={
          showGraph && dataPoints.length > 0 && !isCurrentlyBrewing ? (
            <button
              onClick={clearGraph}
              className="text-xs text-theme-muted hover:text-theme-secondary transition-colors"
            >
              Clear
            </button>
          ) : undefined
        }
      >
        <CardTitle icon={<GaugeIcon className="w-5 h-5" />}>Pressure</CardTitle>
      </CardHeader>
      
      {/* Current pressure display */}
      <div className="flex items-baseline gap-2 mb-4">
        <span className="text-5xl font-bold text-accent tabular-nums">
          {pressure.toFixed(1)}
        </span>
        <span className="text-2xl text-theme-muted">bar</span>
      </div>
      
      {/* Pressure bar */}
      <div className="relative h-4 bg-theme-secondary rounded-full overflow-hidden mb-4">
        <div
          className="absolute inset-y-0 left-0 rounded-full bg-gradient-to-r from-emerald-400 to-cyan-500 transition-all duration-150"
          style={{ width: `${Math.min(100, (pressure / 15) * 100)}%` }}
        />
        <div className="absolute inset-0 flex justify-between px-1 text-[8px] text-theme-muted">
          {[0, 5, 10, 15].map((mark) => (
            <span key={mark} className="relative top-5">
              {mark}
            </span>
          ))}
        </div>
      </div>
      
      {/* Pressure graph during/after brewing */}
      {showGraph && dataPoints.length > 1 && (
        <PressureGraph 
          dataPoints={dataPoints} 
          isLive={isCurrentlyBrewing}
        />
      )}
    </Card>
  );
});

interface PressureGraphProps {
  dataPoints: DataPoint[];
  isLive: boolean;
}

const PressureGraph = memo(function PressureGraph({ dataPoints, isLive }: PressureGraphProps) {
  const width = 280;
  const height = 120;
  const padding = { top: 10, right: 10, bottom: 25, left: 35 };
  const graphWidth = width - padding.left - padding.right;
  const graphHeight = height - padding.top - padding.bottom;
  
  // Calculate time range
  const maxTime = Math.max(GRAPH_DURATION, dataPoints[dataPoints.length - 1]?.time || 0);
  const timeRange = Math.ceil(maxTime / 10) * 10; // Round up to nearest 10s
  
  // Scale functions
  const xScale = (time: number) => padding.left + (time / timeRange) * graphWidth;
  const yScale = (pressure: number) => padding.top + graphHeight - (pressure / MAX_PRESSURE) * graphHeight;
  
  // Generate path
  const pathD = dataPoints.reduce((path, point, i) => {
    const x = xScale(point.time);
    const y = yScale(point.pressure);
    return path + (i === 0 ? `M ${x} ${y}` : ` L ${x} ${y}`);
  }, '');
  
  // Generate area fill path
  const areaD = pathD + 
    ` L ${xScale(dataPoints[dataPoints.length - 1]?.time || 0)} ${yScale(0)}` +
    ` L ${xScale(0)} ${yScale(0)} Z`;
  
  // Y-axis labels
  const yLabels = [0, 3, 6, 9, 12];
  
  // X-axis labels (every 10 seconds)
  const xLabels = Array.from({ length: Math.ceil(timeRange / 10) + 1 }, (_, i) => i * 10);
  
  // Target pressure zone (8-10 bar is ideal for espresso)
  const targetZoneTop = yScale(10);
  const targetZoneBottom = yScale(8);
  const targetZoneHeight = targetZoneBottom - targetZoneTop;

  return (
    <div className="mt-2 border-t border-theme pt-4">
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs font-medium text-theme-secondary">
          {isLive ? "Live Pressure" : "Last Shot"}
        </span>
        {isLive && (
          <span className="flex items-center gap-1.5">
            <span className="w-2 h-2 bg-red-500 rounded-full animate-pulse" />
            <span className="text-xs text-red-500 font-medium">Recording</span>
          </span>
        )}
      </div>
      
      <svg 
        width="100%" 
        height={height} 
        viewBox={`0 0 ${width} ${height}`}
        className="overflow-visible"
      >
        {/* Target pressure zone */}
        <rect
          x={padding.left}
          y={targetZoneTop}
          width={graphWidth}
          height={targetZoneHeight}
          fill="currentColor"
          className="text-emerald-500/10"
        />
        
        {/* Grid lines */}
        {yLabels.map(label => (
          <line
            key={label}
            x1={padding.left}
            y1={yScale(label)}
            x2={width - padding.right}
            y2={yScale(label)}
            stroke="currentColor"
            strokeWidth="1"
            className="text-theme-tertiary"
            strokeDasharray={label === 9 ? "none" : "2,2"}
          />
        ))}
        
        {/* X grid lines */}
        {xLabels.map(label => (
          <line
            key={label}
            x1={xScale(label)}
            y1={padding.top}
            x2={xScale(label)}
            y2={height - padding.bottom}
            stroke="currentColor"
            strokeWidth="1"
            className="text-theme-tertiary"
            strokeDasharray="2,2"
          />
        ))}
        
        {/* Area fill */}
        <path
          d={areaD}
          fill="url(#pressureGradient)"
          opacity="0.3"
        />
        
        {/* Pressure line */}
        <path
          d={pathD}
          fill="none"
          stroke="url(#pressureLineGradient)"
          strokeWidth="2.5"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
        
        {/* Current point indicator (when live) */}
        {isLive && dataPoints.length > 0 && (
          <circle
            cx={xScale(dataPoints[dataPoints.length - 1].time)}
            cy={yScale(dataPoints[dataPoints.length - 1].pressure)}
            r="4"
            fill="currentColor"
            className="text-cyan-500"
          >
            <animate
              attributeName="r"
              values="4;6;4"
              dur="1s"
              repeatCount="indefinite"
            />
          </circle>
        )}
        
        {/* Y-axis labels */}
        {yLabels.map(label => (
          <text
            key={label}
            x={padding.left - 8}
            y={yScale(label)}
            textAnchor="end"
            dominantBaseline="middle"
            className="text-[10px] fill-theme-muted"
          >
            {label}
          </text>
        ))}
        
        {/* X-axis labels */}
        {xLabels.filter(l => l <= timeRange).map(label => (
          <text
            key={label}
            x={xScale(label)}
            y={height - padding.bottom + 15}
            textAnchor="middle"
            className="text-[10px] fill-theme-muted"
          >
            {label}s
          </text>
        ))}
        
        {/* Axis labels */}
        <text
          x={padding.left - 25}
          y={height / 2}
          textAnchor="middle"
          transform={`rotate(-90, ${padding.left - 25}, ${height / 2})`}
          className="text-[9px] fill-theme-muted font-medium"
        >
          bar
        </text>
        
        {/* Gradients */}
        <defs>
          <linearGradient id="pressureGradient" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stopColor="rgb(34, 211, 238)" stopOpacity="0.4" />
            <stop offset="100%" stopColor="rgb(34, 211, 238)" stopOpacity="0" />
          </linearGradient>
          <linearGradient id="pressureLineGradient" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0%" stopColor="rgb(52, 211, 153)" />
            <stop offset="100%" stopColor="rgb(34, 211, 238)" />
          </linearGradient>
        </defs>
      </svg>
      
      {/* Stats summary */}
      {dataPoints.length > 5 && (
        <div className="flex justify-between mt-3 text-xs">
          <div>
            <span className="text-theme-muted">Peak: </span>
            <span className="font-semibold text-theme-secondary">
              {Math.max(...dataPoints.map(d => d.pressure)).toFixed(1)} bar
            </span>
          </div>
          <div>
            <span className="text-theme-muted">Avg: </span>
            <span className="font-semibold text-theme-secondary">
              {(dataPoints.reduce((sum, d) => sum + d.pressure, 0) / dataPoints.length).toFixed(1)} bar
            </span>
          </div>
          <div>
            <span className="text-theme-muted">Time: </span>
            <span className="font-semibold text-theme-secondary">
              {dataPoints[dataPoints.length - 1]?.time.toFixed(1)}s
            </span>
          </div>
        </div>
      )}
    </div>
  );
});
