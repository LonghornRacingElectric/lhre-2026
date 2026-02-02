"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';
import * as d3 from 'd3';

type LatLon = [number, number];
type SectorGate = [LatLon, LatLon];

const TrackMapper = ({ width = 600, height = 400 }: { width?: number; height?: number }) => {
  const [points, setPoints] = useState<LatLon[]>([]);
  const [isDefiningSectors, setIsDefiningSectors] = useState(false);
  const [sectors, setSectors] = useState<SectorGate[]>([]);
  const [pointCount, setPointCount] = useState(0);
  const [hoverLine, setHoverLine] = useState<{ x: number; y: number; angle: number } | null>(null);
  const lastPointRef = useRef<LatLon | null>(null);
  const countRef = useRef(0);

  useEffect(() => {
    // Load points from localStorage on mount to avoid hydration mismatch
    try {
      const saved = localStorage.getItem('trackMapperPoints');
      if (saved) {
        const parsedPoints = JSON.parse(saved) as LatLon[];
        if (Array.isArray(parsedPoints) && parsedPoints.length > 0) {
          setPoints(parsedPoints);
          countRef.current = parsedPoints.length;
          setPointCount(parsedPoints.length);
          lastPointRef.current = parsedPoints[parsedPoints.length - 1];
        }
      }
      // Load sectors from localStorage
      const savedSectors = localStorage.getItem('trackMapperSectors');
      if (savedSectors) {
        const parsedSectors = JSON.parse(savedSectors);
        if (Array.isArray(parsedSectors)) {
          // Check if it's the new format (array of arrays of arrays)
          // New: [[[lat, lon], [lat, lon]], ...]
          if (parsedSectors.length > 0 && Array.isArray(parsedSectors[0]) && Array.isArray(parsedSectors[0][0])) {
             setSectors(parsedSectors as SectorGate[]);
          }
        }
      }
    } catch {
      // ignore localStorage errors
    }
  }, []);

  const kafkaOptions = useMemo(() => ({
    topic: 'track-mapper',
    staleAfterMs: 5000, // consider disconnected if no message in 5 seconds
  }), []);

  // Live connection to Kafka "track-mapper" topic
  const { data: kafkaMsg, connected: isReceivingData } = useKafkaJSON<any>(kafkaOptions);

  /**
   * Normalize incoming Kafka message to [lat, lon]
   * Handles multiple message shapes from main.py:
   * - Direct array: [lat, lon]
   * - Wrapped: { data: [lat, lon] }
   * - Wrapped: { value: [lat, lon] }
   * - Stringified JSON
   */
  const normalize = useCallback((msg: any): LatLon | undefined => {
    let currentMsg = msg;
    // Keep parsing if it's a stringified JSON
    while (typeof currentMsg === 'string') {
      try {
        currentMsg = JSON.parse(currentMsg);
      } catch {
        console.warn('Failed to parse stringified message:', msg);
        return undefined;
      }
    }

    if (!currentMsg) return undefined;

    // Direct array [lat, lon]
    if (Array.isArray(currentMsg) && currentMsg.length >= 2) {
      const lat = Number(currentMsg[0]);
      const lon = Number(currentMsg[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    // Wrapped in { data: [...] }
    if (currentMsg?.data && Array.isArray(currentMsg.data) && currentMsg.data.length >= 2) {
      const lat = Number(currentMsg.data[0]);
      const lon = Number(currentMsg.data[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    // Wrapped in { value: [...] }
    if (currentMsg?.value && Array.isArray(currentMsg.value) && currentMsg.value.length >= 2) {
      const lat = Number(currentMsg.value[0]);
      const lon = Number(currentMsg.value[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    return undefined;
  }, []);

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
  }, [kafkaMsg, normalize]);

  // Format last point for display
  const lastPointText = points.length 
    ? `lat=${points[points.length - 1][0].toFixed(6)}, lon=${points[points.length - 1][1].toFixed(6)}`
    : 'no data';

  const paddingFraction = 0.05;

  // Compute D3 scales and SVG path
  const { xScale, yScale, segments } = useMemo(() => {
    let lonMin = -0.0001, lonMax = 0.0001, latMin = -0.0001, latMax = 0.0001;

    // Combine track and sector points for bounds calculation
    // Flatten sectors (gates) to get all individual points
    const sectorPoints = sectors.flat();
    const allPoints = [...points, ...sectorPoints];

    if (allPoints.length > 0) {
      const lons = allPoints.map((p) => p[1]);
      const lats = allPoints.map((p) => p[0]);
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
      .curve(d3.curveCatmullRom);

    const segs: { d: string, color: string }[] = [];
    const colors = ['#1f77b4', '#ffba7d', '#9ef09e', '#f4afaf', '#c7aae2', '#93e4ef'];

    if (sectors.length > 0) {
        // Map sectors to indices
        const sectorIndices = sectors.map(gate => {
            // Use midpoint of gate to find nearest track point
            const midLat = (gate[0][0] + gate[1][0]) / 2;
            const midLon = (gate[0][1] + gate[1][1]) / 2;

            let bestIdx = 0;
            let minD = Infinity;
            for(let i=0; i<points.length; i++) {
                const d = (points[i][0] - midLat)**2 + (points[i][1] - midLon)**2;
                if(d < minD) { minD = d; bestIdx = i; }
            }
            return bestIdx;
        }).sort((a, b) => a - b);

        let startIdx = 0;
        sectorIndices.forEach((endIdx, i) => {
            const segmentPoints = points.slice(startIdx, endIdx + 1);
            if (segmentPoints.length > 0) {
                segs.push({ d: line(segmentPoints) || '', color: colors[i % colors.length] });
            }
            startIdx = endIdx;
        });
        
        const lastSegmentPoints = points.slice(startIdx);
        if (lastSegmentPoints.length > 0) {
            segs.push({ d: line(lastSegmentPoints) || '', color: colors[sectorIndices.length % colors.length] });
        }
    } else {
        const d = points.length > 0 ? line(points) || '' : '';
        segs.push({ d, color: '#1f77b4' });
    }

    return { xScale: x, yScale: y, segments: segs };
  }, [points, sectors, width, height]);

  // Helper to calculate screen position and normal angle for a point index
  const getPointScreenInfo = useCallback((index: number) => {
    if (!points[index] || !xScale || !yScale) return null;

    const x = xScale(points[index][1]);
    const y = yScale(points[index][0]);

    // Calculate tangent using neighbors
    let prevIdx = index > 0 ? index - 1 : 0;
    let nextIdx = index < points.length - 1 ? index + 1 : points.length - 1;
    
    // Fallback for single point
    if (prevIdx === nextIdx) return { x, y, angle: 0 };

    const pPrev = { x: xScale(points[prevIdx][1]), y: yScale(points[prevIdx][0]) };
    const pNext = { x: xScale(points[nextIdx][1]), y: yScale(points[nextIdx][0]) };
    
    const dx = pNext.x - pPrev.x;
    const dy = pNext.y - pPrev.y;
    
    // Normal vector is tangent + 90 degrees
    const angle = Math.atan2(dy, dx) + Math.PI / 2;
    
    return { x, y, angle };
  }, [points, xScale, yScale]);

  const handleClearPoints = () => {
    if (confirm('Clear all saved track points? This will also clear any defined sectors.')) {
      setPoints([]);
      setPointCount(0);
      countRef.current = 0;
      lastPointRef.current = null;
      setSectors([]); // Also clear sectors
      if (typeof window !== 'undefined') {
        localStorage.removeItem('trackMapperPoints');
        localStorage.removeItem('trackMapperSectors');
      }
    }
  };

  const handleToggleDefineSectors = () => {
    if (!isDefiningSectors && sectors.length === 0 && points.length > 0 && xScale && yScale) {
      // Auto-add start line at index 0
      const info = getPointScreenInfo(0);
      if (info) {
        const length = 60;
        const dx = (length / 2) * Math.cos(info.angle);
        const dy = (length / 2) * Math.sin(info.angle);

        const x1 = info.x - dx;
        const y1 = info.y - dy;
        const x2 = info.x + dx;
        const y2 = info.y + dy;

        const p1: LatLon = [yScale.invert(y1), xScale.invert(x1)];
        const p2: LatLon = [yScale.invert(y2), xScale.invert(x2)];

        const newSectors: SectorGate[] = [[p1, p2]];
        setSectors(newSectors);
        if (typeof window !== 'undefined') {
          localStorage.setItem('trackMapperSectors', JSON.stringify(newSectors));
        }
      }
    }
    setIsDefiningSectors((prev) => !prev);
  };

  const handleClearSectors = () => {
    if (confirm('Are you sure you want to clear all defined sectors?')) {
      setSectors([]);
      if (typeof window !== 'undefined') {
        localStorage.removeItem('trackMapperSectors');
      }
    }
  };

  const handleMouseMove = useCallback((event: React.MouseEvent<SVGSVGElement>) => {
    if (!isDefiningSectors || !xScale || !yScale) return;

    const svg = event.currentTarget;
    const pt = svg.createSVGPoint();
    pt.x = event.clientX;
    pt.y = event.clientY;
    const svgPoint = pt.matrixTransform(svg.getScreenCTM()?.inverse());

    // Find nearest point on track
    let minD = Infinity;
    let bestIdx = -1;
    
    for (let i = 0; i < points.length; i++) {
        const px = xScale(points[i][1]);
        const py = yScale(points[i][0]);
        const d = (px - svgPoint.x) ** 2 + (py - svgPoint.y) ** 2;
        if (d < minD) {
            minD = d;
            bestIdx = i;
        }
    }

    if (bestIdx !== -1) {
        const info = getPointScreenInfo(bestIdx);
        if (info) setHoverLine(info);
    }
  }, [isDefiningSectors, points, xScale, yScale, getPointScreenInfo]);

  const handleMouseLeave = useCallback(() => {
    setHoverLine(null);
  }, []);

  const handleSvgClick = useCallback((event: React.MouseEvent<SVGSVGElement>) => {
    if (!isDefiningSectors || !xScale || !yScale) return;

    // Use the hoverLine info if available, or recalculate nearest point
    if (hoverLine) {
      // Calculate endpoints of the perpendicular line in screen space
      const length = 60; // Same length as visual hover line
      const dx = (length / 2) * Math.cos(hoverLine.angle);
      const dy = (length / 2) * Math.sin(hoverLine.angle);

      const x1 = hoverLine.x - dx;
      const y1 = hoverLine.y - dy;
      const x2 = hoverLine.x + dx;
      const y2 = hoverLine.y + dy;

      // Convert screen endpoints to LatLon
      const p1: LatLon = [yScale.invert(y1), xScale.invert(x1)];
      const p2: LatLon = [yScale.invert(y2), xScale.invert(x2)];

      setSectors((prev: SectorGate[]) => {
        const next: SectorGate[] = [...prev, [p1, p2]];
        if (typeof window !== 'undefined') {
          localStorage.setItem('trackMapperSectors', JSON.stringify(next));
        }
        return next;
      });
    }
  }, [isDefiningSectors, hoverLine, xScale, yScale]);

  return (
    <div className="h-full w-full flex flex-col bg-white">
      {/* Header with status and last point */}
      <div className="p-3 flex items-center justify-between border-b bg-gray-50">
        <div className="flex items-center gap-3">
          <div className={`w-3 h-3 rounded-full transition-colors ${isReceivingData ? 'bg-green-500' : 'bg-gray-400'}`} />
          <div className="text-sm font-medium text-gray-700">{isReceivingData ? '🟢 Live' : '⚫ Disconnected'}</div>
          <div className="text-xs text-gray-500">({pointCount} points, {sectors.length} sectors)</div>
        </div>
        <div className="flex items-center gap-4">
          <div className="text-sm text-gray-600">Last: <span className="font-mono">{lastPointText}</span></div>
          {/* Sector controls */}
          <button
            onClick={handleToggleDefineSectors}
            disabled={points.length === 0}
            className={`px-3 py-1 text-xs rounded transition ${
              isDefiningSectors
                ? 'bg-green-500 text-white hover:bg-green-600'
                : 'bg-blue-500 text-white hover:bg-blue-600'
            } disabled:bg-gray-300 disabled:cursor-not-allowed`}
          >
            {isDefiningSectors ? 'Finish' : 'Define Sectors'}
          </button>
          <button
            onClick={handleClearSectors}
            disabled={sectors.length === 0}
            className="px-3 py-1 text-xs bg-yellow-500 text-white rounded hover:bg-yellow-600 disabled:bg-gray-300 disabled:cursor-not-allowed transition"
          >
            Clear Sectors
          </button>
          <button
            onClick={handleClearPoints}
            disabled={points.length === 0}
            className="px-3 py-1 text-xs bg-red-500 text-white rounded hover:bg-red-600 disabled:bg-gray-300 disabled:cursor-not-allowed transition"
          >
            Clear Track
          </button>
        </div>
      </div>

      {/* SVG visualization area */}
      <div className="flex-grow overflow-hidden relative">
        {points.length > 0 ? (
          <>
            <svg 
              viewBox={`0 0 ${width} ${height}`} 
              className={`w-full h-full ${isDefiningSectors ? 'cursor-crosshair' : ''}`}
              preserveAspectRatio="xMidYMid meet"
              onClick={handleSvgClick}
              onMouseMove={handleMouseMove}
              onMouseLeave={handleMouseLeave}
            >
              {/* Track path - wide stroke to resemble actual track */}
              {segments.map((seg, i) => (
                <path 
                  key={i}
                  d={seg.d} 
                  stroke={(!isDefiningSectors && sectors.length > 0) ? seg.color : '#1f77b4'} 
                  strokeWidth={40}
                  fill="none" 
                  strokeLinecap="butt"
                  strokeLinejoin="round"
                  opacity={0.6}
                />
              ))}
              {/* Sector markers */}
              {sectors.map((gate, index) => {
                const x1 = xScale(gate[0][1]);
                const y1 = yScale(gate[0][0]);
                const x2 = xScale(gate[1][1]);
                const y2 = yScale(gate[1][0]);

                return (
                  <g key={`sector-${index}`}>
                    <line
                      x1={x1}
                      y1={y1}
                      x2={x2}
                      y2={y2}
                      stroke="red"
                      strokeWidth={4}
                    />
                    <text
                      x={x2}
                      y={y2}
                      dx={5}
                      dy={5}
                      fill="red"
                      fontSize="12"
                      fontWeight="bold"
                      textAnchor="start"
                      alignmentBaseline="hanging"
                    >{index === 0 ? 'Start' : index}</text>
                  </g>
                );
              })}
              
              {/* Hover Line */}
              {isDefiningSectors && hoverLine && (
                <g>
                   <line
                      x1={hoverLine.x - (30 * Math.cos(hoverLine.angle))}
                      y1={hoverLine.y - (30 * Math.sin(hoverLine.angle))}
                      x2={hoverLine.x + (30 * Math.cos(hoverLine.angle))}
                      y2={hoverLine.y + (30 * Math.sin(hoverLine.angle))}
                      stroke="rgba(255, 0, 0, 0.7)"
                      strokeWidth={4}
                      strokeDasharray="5,5"
                      pointerEvents="none"
                    />
                </g>
              )}
            </svg>
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

      {/* Sector Coordinates Footer */}
      {sectors.length > 0 && (
        <div className="p-3 border-t bg-gray-50 text-xs overflow-y-auto max-h-32">
            <h3 className="font-bold mb-2 text-gray-700">Sector Gates (Start/End Points)</h3>
            <div className="grid grid-cols-2 gap-2">
                {sectors.map((s, i) => (
                    <div key={i} className="flex items-center gap-2 bg-white p-1 rounded border">
                        <div className="h-4 px-1.5 rounded-full bg-red-500 text-white flex items-center justify-center text-[10px] font-bold shrink-0">
                            {i === 0 ? 'Start' : i}
                        </div>
                        <div className="flex flex-col font-mono text-gray-600">
                            <span>P1: {s[0][0].toFixed(6)}, {s[0][1].toFixed(6)}</span>
                            <span>P2: {s[1][0].toFixed(6)}, {s[1][1].toFixed(6)}</span>
                        </div>
                    </div>
                ))}
            </div>
        </div>
      )}
    </div>
  );
};

export default TrackMapper;