"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';
import * as d3 from 'd3';

type LatLon = [number, number];
type SectorGate = [LatLon, LatLon];

export interface TrackData {
  name: string;
  sectors: SectorGate[];
  points: LatLon[];
  updatedAt: number;
}

const TrackMapper = ({ width = 600, height = 400 }: { width?: number; height?: number }) => {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const [points, setPoints] = useState<LatLon[]>([]);
  const [isDefiningSectors, setIsDefiningSectors] = useState(false);
  const [sectors, setSectors] = useState<SectorGate[]>([]);
  const [pointCount, setPointCount] = useState(0);
  const [hoverLine, setHoverLine] = useState<{ x: number; y: number; angle: number; index: number } | null>(null);
  const [isRecording, setIsRecording] = useState(true);
  const [isSettingStartGate, setIsSettingStartGate] = useState(false);
  const [isSavingToDb, setIsSavingToDb] = useState(false);
  const [saveDbStatus, setSaveDbStatus] = useState<{ ok: boolean; msg: string } | null>(null);

  // Track Management State
  const [trackName, setTrackName] = useState<string>('Untitled');
  const [availableTracks, setAvailableTracks] = useState<string[]>([]);
  const lastSavedNameRef = useRef<string>(trackName);

  const lastPointRef = useRef<LatLon | null>(null);
  const countRef = useRef(0);

  useEffect(() => {
    // Load available tracks from localStorage on mount
    if (typeof window !== 'undefined') {
      const tracks: string[] = [];
      for (let i = 0; i < localStorage.length; i++) {
        const key = localStorage.key(i);
        if (key?.startsWith('track_data_')) {
          tracks.push(key.replace('track_data_', ''));
        }
      }
      setAvailableTracks(tracks.sort());

      // Try to load the last active track
      const lastActive = localStorage.getItem('track_mapper_last_active');
      if (lastActive && tracks.includes(lastActive)) {
        loadTrackData(lastActive);
      }
    }
  }, []);

  const loadTrackData = (name: string) => {
    try {
      const saved = localStorage.getItem(`track_data_${name}`);
      if (saved) {
        const data = JSON.parse(saved) as TrackData;
        setTrackName(data.name);
        setPoints(data.points || []);
        setSectors(data.sectors || []);
        
        setIsRecording(false);
        // Update refs for continuity
        countRef.current = data.points?.length || 0;
        setPointCount(countRef.current);
        lastPointRef.current = data.points?.[data.points.length - 1] || null;
        lastSavedNameRef.current = data.name;
        
        // Save as last active
        localStorage.setItem('track_mapper_last_active', data.name);
      }
    } catch (e) {
      console.error("Failed to load track data", e);
    }
  };

  const handleTrackChange = (name: string) => {
    if (name === 'NEW_TRACK_OPTION') {
      // Reset to new track
      setTrackName('Untitled');
      setPoints([]);
      setSectors([]);
      countRef.current = 0;
      setIsRecording(true);
      setPointCount(0);
      lastPointRef.current = null;
      lastSavedNameRef.current = 'Untitled';
    } else {
      loadTrackData(name);
    }
  };

  const handleDeleteTrack = () => {
    if (confirm(`Are you sure you want to delete track "${trackName}"?`)) {
      localStorage.removeItem(`track_data_${trackName}`);
      setAvailableTracks(prev => prev.filter(t => t !== trackName));
      handleTrackChange('NEW_TRACK_OPTION');
    }
  };

  const handleSaveAs = () => {
    const newName = prompt("Enter name for copy:", `${trackName} (Copy)`);
    if (newName && newName.trim() !== "") {
      if (availableTracks.includes(newName)) {
        if (!confirm(`Track "${newName}" already exists. Overwrite?`)) {
          return;
        }
      }
      // Update ref to prevent deletion of current track in the auto-save useEffect
      lastSavedNameRef.current = newName;
      setTrackName(newName);
    }
  };

  const kafkaOptions = useMemo(() => ({
    topic: 'track-mapper',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 5000, // consider disconnected if no message in 5 seconds
  }), [matchesSelectedCar, selectedCar, ssePath]);

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
    if (!isRecording) return;

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
      return next;
    });
    lastPointRef.current = p;
    countRef.current += 1;
    setPointCount(countRef.current);
    
    console.log(`✓ Point ${countRef.current}: [lat=${p[0].toFixed(6)}, lon=${p[1].toFixed(6)}]`);
  }, [kafkaMsg, normalize, isRecording]);

  // Auto-save effect: Saves whenever points, sectors, or name changes
  useEffect(() => {
    if (typeof window !== 'undefined' && (points.length > 0 || sectors.length > 0)) {
      const data: TrackData = {
        name: trackName,
        sectors,
        points,
        updatedAt: Date.now(),
      };

      const oldName = lastSavedNameRef.current;
      if (oldName && oldName !== trackName) {
        localStorage.removeItem(`track_data_${oldName}`);
      }

      localStorage.setItem(`track_data_${trackName}`, JSON.stringify(data));
      localStorage.setItem('track_mapper_last_active', trackName);

      setAvailableTracks(prev => {
        let next = prev;
        if (oldName && oldName !== trackName) {
          next = next.filter(t => t !== oldName);
        }
        if (!next.includes(trackName)) {
          next = [...next, trackName].sort();
        }
        return next;
      });
      
      lastSavedNameRef.current = trackName;
    }
  }, [points, sectors, trackName]);

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
    const prevIdx = index > 0 ? index - 1 : 0;
    const nextIdx = index < points.length - 1 ? index + 1 : points.length - 1;
    
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
      setIsRecording(true);
    }
  };

  const handleFinishMapping = () => {
    if (confirm('Finish mapping this track? This will stop recording new points.')) {
      setIsRecording(false);
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
      }
    }
    setIsDefiningSectors((prev) => !prev);
  };

  const handleToggleSetStartGate = () => {
    setIsSettingStartGate((prev) => !prev);
    setIsDefiningSectors(false);
  };

  const handleSvgClickStartGate = useCallback((_event: React.MouseEvent<SVGSVGElement>) => {
    if (!isSettingStartGate || !xScale || !yScale) return;

    if (hoverLine) {
      const length = 60;
      const dx = (length / 2) * Math.cos(hoverLine.angle);
      const dy = (length / 2) * Math.sin(hoverLine.angle);

      const p1: LatLon = [yScale.invert(hoverLine.y - dy), xScale.invert(hoverLine.x - dx)];
      const p2: LatLon = [yScale.invert(hoverLine.y + dy), xScale.invert(hoverLine.x + dx)];

      setSectors((prev) => {
        // Replace sectors[0] (start gate); keep remaining sector gates
        const rest = prev.slice(1);
        return [[p1, p2], ...rest];
      });
      setIsSettingStartGate(false);
      setSaveDbStatus(null);
    }
  }, [isSettingStartGate, hoverLine, xScale, yScale]);

  const handleSaveToDatabase = async () => {
    if (!sectors[0]) return;
    setIsSavingToDb(true);
    setSaveDbStatus(null);
    try {
      const res = await fetch('/api/save-track-mapping', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          name: trackName.trim(),
          start_gate: sectors[0],
          points,
          sectors,
        }),
      });
      const json = await res.json();
      if (res.ok) {
        setSaveDbStatus({ ok: true, msg: json.message });
      } else {
        setSaveDbStatus({ ok: false, msg: json.error || 'Unknown error' });
      }
    } catch {
      setSaveDbStatus({ ok: false, msg: 'Network error' });
    } finally {
      setIsSavingToDb(false);
    }
  };

  const handleClearSectors = () => {
    if (confirm('Are you sure you want to clear all defined sectors?')) {
      setSectors([]);
    }
  };

  const handleMouseMove = useCallback((event: React.MouseEvent<SVGSVGElement>) => {
    if ((!isDefiningSectors && !isSettingStartGate) || !xScale || !yScale) return;

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
        if (info) setHoverLine({ ...info, index: bestIdx });
    }
  }, [isDefiningSectors, isSettingStartGate, points, xScale, yScale, getPointScreenInfo]);

  const handleMouseLeave = useCallback(() => {
    setHoverLine(null);
  }, []);

  const handleSvgClick = useCallback((event: React.MouseEvent<SVGSVGElement>) => {
    if (!isDefiningSectors || !xScale || !yScale) return;

    // Use the hoverLine info if available, or recalculate nearest point
    if (hoverLine) {
      // Check if sector already exists nearby
      const currentIndex = hoverLine.index;
      const isTooClose = sectors.some(gate => {
        const midLat = (gate[0][0] + gate[1][0]) / 2;
        const midLon = (gate[0][1] + gate[1][1]) / 2;
        
        // Find nearest point index for this gate
        let bestIdx = 0;
        let minD = Infinity;
        for(let i=0; i<points.length; i++) {
            const d = (points[i][0] - midLat)**2 + (points[i][1] - midLon)**2;
            if(d < minD) { minD = d; bestIdx = i; }
        }
        return Math.abs(bestIdx - currentIndex) < 10;
      });

      if (isTooClose) {
        return;
      }

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
        return next;
      });
    }
  }, [isDefiningSectors, hoverLine, xScale, yScale, sectors, points]);

  return (
    <div className="h-full w-full flex flex-col bg-white">
      {/* Header with status and last point */}
      <div className="p-3 flex items-center justify-between border-b bg-gray-50">
        <div className="flex items-center gap-3">
          <div className={`w-3 h-3 rounded-full transition-colors ${isReceivingData ? 'bg-green-500' : 'bg-gray-400'}`} />
          <div className="text-sm font-medium text-gray-700">
            {isReceivingData 
              ? (isRecording ? `🟢 Live ${selectedCarLabel} (Recording)` : `🟡 Live ${selectedCarLabel} (View Only)`) 
              : '⚫ Disconnected'}
          </div>
          <div className="text-xs text-gray-500">({pointCount} points, {sectors.length} sectors)</div>
        </div>
        <div className="flex items-center gap-4">
          {/* Track Selection UI */}
          <div className="flex items-center gap-2 border-r pr-4 mr-2">
            <select 
              value={availableTracks.includes(trackName) ? trackName : 'NEW_TRACK_OPTION'} 
              onChange={(e) => handleTrackChange(e.target.value)}
              className="border rounded px-2 py-1 text-xs bg-white"
            >
              <option value="NEW_TRACK_OPTION">-- New / Unsaved --</option>
              {availableTracks.map(t => (
                <option key={t} value={t}>{t}</option>
              ))}
            </select>
            <input 
              type="text" 
              value={trackName}
              onChange={(e) => setTrackName(e.target.value)}
              className="border rounded px-2 py-1 text-xs w-32"
              placeholder="Track Name"
            />
            <button onClick={handleDeleteTrack} className="text-gray-400 hover:text-red-500" title="Delete Track">
              <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
            </button>
            <button onClick={handleSaveAs} className="text-gray-400 hover:text-blue-500 ml-1" title="Save As Copy">
              <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path><polyline points="17 21 17 13 7 13 7 21"></polyline><polyline points="7 3 7 8 15 8"></polyline></svg>
            </button>
          </div>

          <div className="text-sm text-gray-600">Last: <span className="font-mono">{lastPointText}</span></div>
          {/* Sector controls */}
          {isRecording && points.length > 0 && (
            <button
              onClick={handleFinishMapping}
              className="px-3 py-1 text-xs bg-indigo-500 text-white rounded hover:bg-indigo-600 transition"
            >
              Finish Mapping
            </button>
          )}
          {/* Mandatory: Set Start Gate */}
          <button
            onClick={handleToggleSetStartGate}
            disabled={points.length === 0}
            className={`px-3 py-1 text-xs rounded transition font-semibold border-2 ${
              isSettingStartGate
                ? 'bg-green-600 text-white border-green-700 hover:bg-green-700'
                : sectors[0]
                ? 'bg-green-100 text-green-800 border-green-400 hover:bg-green-200'
                : 'bg-orange-500 text-white border-orange-600 hover:bg-orange-600 animate-pulse'
            } disabled:bg-gray-300 disabled:border-gray-300 disabled:cursor-not-allowed disabled:animate-none`}
            title={sectors[0] ? 'Start gate is set — click to reposition' : 'Required: click to place the start gate on the track'}
          >
            {isSettingStartGate ? 'Click track to place…' : sectors[0] ? 'Start Gate ✓' : 'Set Start Gate *'}
          </button>
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
          {/* Save to Database — only enabled once start gate is placed */}
          <div className="flex flex-col items-end gap-1">
            <button
              onClick={handleSaveToDatabase}
              disabled={!sectors[0] || isSavingToDb || points.length === 0}
              className="px-3 py-1 text-xs bg-gray-800 text-white rounded hover:bg-black disabled:bg-gray-300 disabled:cursor-not-allowed transition"
              title={!sectors[0] ? 'Set a start gate before saving' : 'Save track mapping to database'}
            >
              {isSavingToDb ? 'Saving…' : 'Save to DB'}
            </button>
            {saveDbStatus && (
              <span className={`text-[10px] ${saveDbStatus.ok ? 'text-green-600' : 'text-red-500'}`}>
                {saveDbStatus.msg}
              </span>
            )}
          </div>
        </div>
      </div>

      {/* SVG visualization area */}
      <div className="flex-grow overflow-hidden relative">
        {points.length > 0 ? (
          <>
            <svg
              viewBox={`0 0 ${width} ${height}`}
              className={`w-full h-full ${(isDefiningSectors || isSettingStartGate) ? 'cursor-crosshair' : ''}`}
              preserveAspectRatio="xMidYMid meet"
              onClick={isSettingStartGate ? handleSvgClickStartGate : handleSvgClick}
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
                const isStartGate = index === 0;

                return (
                  <g key={`sector-${index}`}>
                    <line
                      x1={x1}
                      y1={y1}
                      x2={x2}
                      y2={y2}
                      stroke={isStartGate ? '#16a34a' : 'red'}
                      strokeWidth={isStartGate ? 5 : 4}
                    />
                    <text
                      x={x2}
                      y={y2}
                      dx={5}
                      dy={5}
                      fill={isStartGate ? '#16a34a' : 'red'}
                      fontSize="12"
                      fontWeight="bold"
                      textAnchor="start"
                      alignmentBaseline="hanging"
                    >{isStartGate ? 'START' : index}</text>
                  </g>
                );
              })}
              
              {/* Hover Line */}
              {(isDefiningSectors || isSettingStartGate) && hoverLine && (
                <g>
                  <line
                    x1={hoverLine.x - (30 * Math.cos(hoverLine.angle))}
                    y1={hoverLine.y - (30 * Math.sin(hoverLine.angle))}
                    x2={hoverLine.x + (30 * Math.cos(hoverLine.angle))}
                    y2={hoverLine.y + (30 * Math.sin(hoverLine.angle))}
                    stroke={isSettingStartGate ? 'rgba(22, 163, 74, 0.8)' : 'rgba(255, 0, 0, 0.7)'}
                    strokeWidth={isSettingStartGate ? 5 : 4}
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
        <div className="p-3 border-t bg-gray-50 text-xs overflow-y-auto max-h-60">
            <h3 className="font-bold mb-2 text-gray-700">Gates — <span className="text-green-700">Start Gate</span> + Sector Gates</h3>
            <div className="grid grid-cols-2 gap-2">
                {sectors.map((s, i) => (
                    <div key={i} className="flex items-center gap-2 bg-white p-1 rounded border">
                        <div className={`h-4 px-1.5 rounded-full text-white flex items-center justify-center text-[10px] font-bold shrink-0 ${i === 0 ? 'bg-green-600' : 'bg-red-500'}`}>
                            {i === 0 ? 'START' : i}
                        </div>
                        <div className="flex flex-col font-mono text-gray-600">
                            <span>P1: {s[0][0].toFixed(6)}, {s[0][1].toFixed(6)}</span>
                            <span>P2: {s[1][0].toFixed(6)}, {s[1][1].toFixed(6)}</span>
                        </div>
                    </div>
                ))}
                {points.length > 0 && (
                  <div className="flex items-center gap-2 bg-white p-1 rounded border border-blue-300">
                      <div className="h-4 px-1.5 rounded-full bg-blue-500 text-white flex items-center justify-center text-[10px] font-bold shrink-0">
                          Track End
                      </div>
                      <div className="flex flex-col font-mono text-gray-600">
                          <span>Lat: {points[points.length - 1][0].toFixed(6)}</span>
                          <span>Lon: {points[points.length - 1][1].toFixed(6)}</span>
                      </div>
                  </div>
                )}
            </div>
        </div>
      )}
    </div>
  );
};

export default TrackMapper;
