import { useEffect, useMemo, useRef, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';
import * as d3 from 'd3';

type GGPoint = { id: number; x: number; y: number };
type GGMessage = { data?: { x?: number; y?: number } };

const MAX_POINTS = 700;
const PLOT_RANGE_G = 2.5;
const TICK_STEP_G = 0.5;

const GGPlot = () => {
  const [points, setPoints] = useState<GGPoint[]>([]);
  const { selectedCar, ssePath, matchesSelectedCar } = useCarSelection();
  const nextPointIdRef = useRef(1);

  useKafkaJSON<GGMessage>({
    topic: 'gg-plot',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 2000,
    sampleMs: 80,
    onMessage: (_evt, parsed) => {
      const x = Number(parsed?.data?.x);
      const y = Number(parsed?.data?.y);
      if (Number.isFinite(x) && Number.isFinite(y)) {
        setPoints((prevPoints) => [
          ...prevPoints.slice(-(MAX_POINTS - 1)),
          { id: nextPointIdRef.current++, x, y },
        ]);
      }
    },
  });

  useEffect(() => {
    nextPointIdRef.current = 1;
    setPoints([]);
  }, [selectedCar]);

  const width = 560;
  const height = 560;
  const margin = { top: 20, right: 24, bottom: 42, left: 48 };
  const innerWidth = width - margin.left - margin.right;
  const innerHeight = height - margin.top - margin.bottom;

  const tickValues = useMemo(
    () => d3.range(-PLOT_RANGE_G, PLOT_RANGE_G + 1e-6, TICK_STEP_G),
    [],
  );

  const xScale = useMemo(
    () =>
      d3
        .scaleLinear()
        .domain([-PLOT_RANGE_G, PLOT_RANGE_G])
        .range([margin.left, margin.left + innerWidth]),
    [innerWidth, margin.left],
  );

  const yScale = useMemo(
    () =>
      d3
        .scaleLinear()
        .domain([-PLOT_RANGE_G, PLOT_RANGE_G])
        .range([margin.top + innerHeight, margin.top]),
    [innerHeight, margin.top],
  );

  return (
    <div className="bg-white rounded-lg shadow-md p-4 w-full h-full flex flex-col">
      <h2 className="text-lg font-bold mb-4">GG Plot</h2>

      <div className="relative flex-grow">
        <svg viewBox={`0 0 ${width} ${height}`} className="w-full h-full">
          <rect x={margin.left} y={margin.top} width={innerWidth} height={innerHeight} fill="#f8fafc" />

          {tickValues.map((tick) => (
            <line
              key={`v-${tick}`}
              x1={xScale(tick)}
              y1={margin.top}
              x2={xScale(tick)}
              y2={margin.top + innerHeight}
              stroke={tick === 0 ? '#64748b' : '#e2e8f0'}
              strokeWidth={tick === 0 ? 1.5 : 1}
            />
          ))}
          {tickValues.map((tick) => (
            <line
              key={`h-${tick}`}
              x1={margin.left}
              y1={yScale(tick)}
              x2={margin.left + innerWidth}
              y2={yScale(tick)}
              stroke={tick === 0 ? '#64748b' : '#e2e8f0'}
              strokeWidth={tick === 0 ? 1.5 : 1}
            />
          ))}

          {tickValues.map((tick) => (
            <text
              key={`xt-${tick}`}
              x={xScale(tick)}
              y={height - 14}
              textAnchor="middle"
              className="fill-slate-500 text-[10px]"
            >
              {tick.toFixed(1)}
            </text>
          ))}
          {tickValues.map((tick) => (
            <text
              key={`yt-${tick}`}
              x={margin.left - 8}
              y={yScale(tick) + 3}
              textAnchor="end"
              className="fill-slate-500 text-[10px]"
            >
              {tick.toFixed(1)}
            </text>
          ))}

          {points.map((point) => (
            <circle
              key={point.id}
              cx={xScale(point.x)}
              cy={yScale(point.y)}
              r={2}
              fill="#2563eb"
              opacity={0.8}
            />
          ))}

          <text x={width / 2} y={height - 2} textAnchor="middle" className="fill-slate-600 text-[11px]">
            Lateral Accel (g)
          </text>
          <text
            x={14}
            y={height / 2}
            textAnchor="middle"
            transform={`rotate(-90 14 ${height / 2})`}
            className="fill-slate-600 text-[11px]"
          >
            Longitudinal Accel (g)
          </text>
        </svg>
        {points.length === 0 ? (
          <p className="absolute inset-0 flex items-center justify-center text-gray-500 pointer-events-none">
            Waiting for gg-plot data...
          </p>
        ) : null}
      </div>
    </div>
  );
};

export default GGPlot;
