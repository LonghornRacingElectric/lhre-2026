"use client";

import { useEffect, useMemo, useRef, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';
import * as d3 from 'd3';

type LatLon = [number, number];

const TrackMapper = ({ width = 600, height = 400 }: { width?: number; height?: number }) => {
  const [points, setPoints] = useState<LatLon[]>(() => {
    // Load points from localStorage on mount
    if (typeof window !== 'undefined') {
      try {
        const saved = localStorage.getItem('trackMapperPoints');
        return saved ? JSON.parse(saved) : [];
      } catch {
        return [];
      }
    }
    return [];
  });
  const [pointCount, setPointCount] = useState(0);
  const [hoveredPoint, setHoveredPoint] = useState<number | null>(null);
  const [mousePos, setMousePos] = useState<{ x: number; y: number }>({ x: 0, y: 0 });
  const lastPointRef = useRef<LatLon | null>(null);
  const countRef = useRef(0);

  // Live connection to Kafka "track-mapper" topic
  const { data: kafkaMsg, kafkaConnected } = useKafkaJSON<any>({ 
    topic: 'track-mapper',
    staleAfterMs: 5000, // consider disconnected if no message in 5 seconds
  });

  /**
   * Normalize incoming Kafka message to [lat, lon]
   * Handles multiple message shapes from main.py:
   * - Direct array: [lat, lon]
   * - Wrapped: { data: [lat, lon] }
   * - Wrapped: { value: [lat, lon] }
   * - Stringified JSON
   */
  const normalize = (msg: any): LatLon | undefined => {
    if (!msg) return undefined;

    // Direct array [lat, lon]
    if (Array.isArray(msg) && msg.length >= 2) {
      const lat = Number(msg[0]);
      const lon = Number(msg[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    // Wrapped in { data: [...] }
    if (msg?.data && Array.isArray(msg.data) && msg.data.length >= 2) {
      const lat = Number(msg.data[0]);
      const lon = Number(msg.data[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    // Wrapped in { value: [...] }
    if (msg?.value && Array.isArray(msg.value) && msg.value.length >= 2) {
      const lat = Number(msg.value[0]);
      const lon = Number(msg.value[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    // Try to parse if it's a stringified JSON
    if (typeof msg === 'string') {
      try {
        const parsed = JSON.parse(msg);
        return normalize(parsed);
      } catch {
        console.warn('Failed to parse stringified message:', msg);
        return undefined;
      }
    }

    return undefined;
  };

  // Append only new points from Kafka stream
  useEffect(() => {
    const p = normalize(kafkaMsg);
    
    if (!p) {
      if (kafkaMsg !== undefined && kafkaMsg !== null) {
        console.warn('Failed to normalize kafka message:', kafkaMsg);
      }
      return;
    }

    // Skip duplicate consecutive points
    const last = lastPointRef.current;
    if (last && last[0] === p[0] && last[1] === p[1]) {
      return;
    }

    // Append new point
    setPoints((prev) => {
      const next = [...prev, p];
      // Save to localStorage
      if (typeof window !== 'undefined') {
        localStorage.setItem('trackMapperPoints', JSON.stringify(next));
      }
      return next;
    });
    lastPointRef.current = p;
    countRef.current += 1;
    setPointCount(countRef.current);
    
    console.log(`✓ Point ${countRef.current}: [lat=${p[0].toFixed(6)}, lon=${p[1].toFixed(6)}]`);
  }, [kafkaMsg]);

  // Format last point for display
  const lastPointText = points.length 
    ? `lat=${points[points.length - 1][0].toFixed(6)}, lon=${points[points.length - 1][1].toFixed(6)}`
    : 'no data';

  const handleClearPoints = () => {
    if (confirm('Clear all saved points?')) {
      setPoints([]);
      setPointCount(0);
      countRef.current = 0;
      lastPointRef.current = null;
      if (typeof window !== 'undefined') {
        localStorage.removeItem('trackMapperPoints');
      }
    }
  };

  const paddingFraction = 0.05;

  // Compute D3 scales and SVG path
  const { xScale, yScale, pathD } = useMemo(() => {
    let lonMin = -0.0001, lonMax = 0.0001, latMin = -0.0001, latMax = 0.0001;

    if (points.length > 0) {
      const lons = points.map((p) => p[1]);
      const lats = points.map((p) => p[0]);
      lonMin = Math.min(...lons);
      lonMax = Math.max(...lons);
      latMin = Math.min(...lats);
      latMax = Math.max(...lats);

      // Add padding to avoid points on the edge
      const lonRange = lonMax - lonMin;
      const latRange = latMax - latMin;
      const lonPad = (lonRange === 0 ? 0.0001 : lonRange) * paddingFraction;
      const latPad = (latRange === 0 ? 0.0001 : latRange) * paddingFraction;
      lonMin -= lonPad; lonMax += lonPad;
      latMin -= latPad; latMax += latPad;
    }

    const x = d3.scaleLinear().domain([lonMin, lonMax]).range([0, width]);
    const y = d3.scaleLinear().domain([latMin, latMax]).range([height, 0]);
    
    // Use D3 line generator with smooth curve interpolation
    // Note: p[0] = latitude (y-axis), p[1] = longitude (x-axis)
    const line = d3.line<LatLon>()
      .x((p) => x(p[1]))
      .y((p) => y(p[0]))
      .curve(d3.curveNatural); // Smooth curves instead of sharp angles

    const d = points.length > 0 ? line(points) || '' : '';

    return { xScale: x, yScale: y, pathD: d };
  }, [points, width, height]);

  return (
    <div className="h-full w-full flex flex-col bg-white">
      {/* Header with status and last point */}
      <div className="p-3 flex items-center justify-between border-b bg-gray-50">
        <div className="flex items-center gap-3">
          <div className={`w-3 h-3 rounded-full transition-colors ${kafkaConnected ? 'bg-green-500' : 'bg-gray-400'}`} />
          <div className="text-sm font-medium text-gray-700">{kafkaConnected ? '🟢 Live' : '⚫ Disconnected'}</div>
          <div className="text-xs text-gray-500">({pointCount} points)</div>
        </div>
        <div className="flex items-center gap-4">
          <div className="text-sm text-gray-600">Last: <span className="font-mono">{lastPointText}</span></div>
          <button
            onClick={handleClearPoints}
            disabled={points.length === 0}
            className="px-3 py-1 text-xs bg-red-500 text-white rounded hover:bg-red-600 disabled:bg-gray-300 disabled:cursor-not-allowed transition"
          >
            Clear
          </button>
        </div>
      </div>

      {/* SVG visualization area */}
      <div className="flex-grow overflow-hidden relative">
        {points.length > 0 ? (
          <>
            <svg 
              viewBox={`0 0 ${width} ${height}`} 
              className="w-full h-full" 
              preserveAspectRatio="xMidYMid meet"
              onMouseMove={(e) => setMousePos({ x: e.clientX, y: e.clientY })}
              onMouseLeave={() => setHoveredPoint(null)}
            >
              {/* Track path - wide stroke to resemble actual track */}
              <path 
                d={pathD} 
                stroke="#1f77b4" 
                strokeWidth={30} 
                fill="none" 
                strokeLinecap="round" 
                strokeLinejoin="round"
                opacity={0.99}
              />
              
              {/* Points */}
              {points.map((p, idx) => (
                <circle
                  key={idx}
                  cx={xScale(p[1])}
                  cy={yScale(p[0])}
                  r={hoveredPoint === idx ? 6 : 3}
                  fill={idx === points.length - 1 ? '#ef4444' : '#3b82f6'}
                  opacity={hoveredPoint === idx ? 1 : 0.7}
                  style={{ 
                    transition: 'r 0.15s, opacity 0.15s',
                    cursor: 'pointer'
                  }}
                  onMouseEnter={() => setHoveredPoint(idx)}
                  onMouseLeave={() => setHoveredPoint(null)}
                />
              ))}
            </svg>

            {/* Tooltip */}
            {hoveredPoint !== null && (
              <div
                className="fixed bg-gray-900 text-white px-3 py-2 rounded shadow-lg text-xs font-mono z-50 pointer-events-none"
                style={{
                  left: `${mousePos.x + 10}px`,
                  top: `${mousePos.y + 10}px`,
                }}
              >
                <div className="font-semibold">Point #{hoveredPoint + 1}</div>
                <div>Lat: {points[hoveredPoint][0].toFixed(8)}</div>
                <div>Lon: {points[hoveredPoint][1].toFixed(8)}</div>
              </div>
            )}
          </>
        ) : (
          <div className="w-full h-full flex items-center justify-center text-gray-400">
            <div className="text-center">
              <div className="text-lg">📍 No GPS points yet</div>
              <div className="text-sm mt-2">Waiting for track-mapper Kafka topic...</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default TrackMapper;