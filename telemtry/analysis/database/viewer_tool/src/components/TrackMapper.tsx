"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';
import { useCarSelection } from '@/lib/carSelection';
import * as d3 from 'd3';

type LatLon = [number, number];
type SectorGate = [LatLon, LatLon];

function parseCSVLine(line: string): string[] {
  const fields: string[] = [];
  let cur = '';
  let inQuotes = false;
  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    if (ch === '"') {
      if (inQuotes && line[i + 1] === '"') { cur += '"'; i++; }
      else { inQuotes = !inQuotes; }
    } else if (ch === ',' && !inQuotes) {
      fields.push(cur); cur = '';
    } else {
      cur += ch;
    }
  }
  fields.push(cur);
  return fields;
}

export interface TrackData {
  name: string;
  sectors: SectorGate[];
  points: (LatLon | null)[];
  updatedAt: number;
}

const TrackMapper = ({ width = 600, height = 400, isFullscreen = false }: { width?: number; height?: number; isFullscreen?: boolean }) => {
  const { selectedCar, selectedCarLabel, ssePath, matchesSelectedCar } = useCarSelection();
  const [points, setPoints] = useState<(LatLon | null)[]>([]);
  const [isDefiningSectors, setIsDefiningSectors] = useState(false);
  const [sectors, setSectors] = useState<SectorGate[]>([]);
  const [pointCount, setPointCount] = useState(0);
  const [hoverLine, setHoverLine] = useState<{ x: number; y: number; angle: number; index: number } | null>(null);
  const [isRecording, setIsRecording] = useState(true);
  const [isSettingStartGate, setIsSettingStartGate] = useState(false);
  const [isSavingToDb, setIsSavingToDb] = useState(false);
  const [saveDbStatus, setSaveDbStatus] = useState<{ ok: boolean; msg: string } | null>(null);
  const [isLoadingCSV, setIsLoadingCSV] = useState(false);
  const csvInputRef = useRef<HTMLInputElement>(null);

  // Track Management State
  const [trackName, setTrackName] = useState<string>('Untitled');
  const [availableTracks, setAvailableTracks] = useState<string[]>([]);
  const lastSavedNameRef = useRef<string>(trackName);

  const lastPointRef = useRef<LatLon | null>(null);
  const countRef = useRef(0);
  const needsAutoGateRef = useRef(false);

  useEffect(() => {
    if (typeof window !== 'undefined') {
      const tracks: string[] = [];
      for (let i = 0; i < localStorage.length; i++) {
        const key = localStorage.key(i);
        if (key?.startsWith('track_data_')) {
          tracks.push(key.replace('track_data_', ''));
        }
      }
      setAvailableTracks(tracks.sort());

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
        const savedPoints = (data.points || []) as (LatLon | null)[];
        setTrackName(data.name);
        setPoints(savedPoints);
        setSectors(data.sectors || []);
        setIsRecording(false);
        const validCount = savedPoints.filter(p => p !== null).length;
        countRef.current = validCount;
        setPointCount(validCount);
        lastPointRef.current = [...savedPoints].reverse().find((p): p is LatLon => p !== null) ?? null;
        lastSavedNameRef.current = data.name;
        localStorage.setItem('track_mapper_last_active', data.name);
      }
    } catch (e) {
      console.error("Failed to load track data", e);
    }
  };

  const handleTrackChange = (name: string) => {
    if (name === 'NEW_TRACK_OPTION') {
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
      lastSavedNameRef.current = newName;
      setTrackName(newName);
    }
  };

  const kafkaOptions = useMemo(() => ({
    topic: 'track-mapper',
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    staleAfterMs: 5000,
  }), [matchesSelectedCar, selectedCar, ssePath]);

  const { data: kafkaMsg, connected: isReceivingData } = useKafkaJSON<any>(kafkaOptions);

  const normalize = useCallback((msg: any): LatLon | undefined => {
    let currentMsg = msg;
    while (typeof currentMsg === 'string') {
      try {
        currentMsg = JSON.parse(currentMsg);
      } catch {
        console.warn('Failed to parse stringified message:', msg);
        return undefined;
      }
    }

    if (!currentMsg) return undefined;

    if (Array.isArray(currentMsg) && currentMsg.length >= 2) {
      const lat = Number(currentMsg[0]);
      const lon = Number(currentMsg[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    if (currentMsg?.data && Array.isArray(currentMsg.data) && currentMsg.data.length >= 2) {
      const lat = Number(currentMsg.data[0]);
      const lon = Number(currentMsg.data[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    if (currentMsg?.value && Array.isArray(currentMsg.value) && currentMsg.value.length >= 2) {
      const lat = Number(currentMsg.value[0]);
      const lon = Number(currentMsg.value[1]);
      if (!isNaN(lat) && !isNaN(lon)) return [lat, lon];
    }

    return undefined;
  }, []);

  useEffect(() => {
    if (!isRecording) return;

    const p = normalize(kafkaMsg);

    if (!p) {
      if (kafkaMsg !== undefined && kafkaMsg !== null) {
        console.warn('Failed to normalize kafka message:', kafkaMsg);
      }
      return;
    }

    const last = lastPointRef.current;
    if (last && last[0] === p[0] && last[1] === p[1]) {
      return;
    }

    setPoints((prev) => [...prev, p]);
    lastPointRef.current = p;
    countRef.current += 1;
    setPointCount(countRef.current);

    console.log(`✓ Point ${countRef.current}: [lat=${p[0].toFixed(6)}, lon=${p[1].toFixed(6)}]`);
  }, [kafkaMsg, normalize, isRecording]);

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

  const lastValidPoint = [...points].reverse().find((p): p is LatLon => p !== null);
  const lastPointText = lastValidPoint
    ? `lat=${lastValidPoint[0].toFixed(6)}, lon=${lastValidPoint[1].toFixed(6)}`
    : 'no data';

  const paddingFraction = 0.05;

  const { xScale, yScale, segments } = useMemo(() => {
    let lonMin = -0.0001, lonMax = 0.0001, latMin = -0.0001, latMax = 0.0001;

    const sectorPoints = sectors.flat();
    const validPoints = points.filter((p): p is LatLon => p !== null);
    const allPoints = [...validPoints, ...sectorPoints];

    if (allPoints.length > 0) {
      const lons = allPoints.map((p) => p[1]);
      const lats = allPoints.map((p) => p[0]);
      lonMin = Math.min(...lons);
      lonMax = Math.max(...lons);
      latMin = Math.min(...lats);
      latMax = Math.max(...lats);

      const lonRange = lonMax - lonMin;
      const latRange = latMax - latMin;
      const lonPad = (lonRange === 0 ? 0.0001 : lonRange) * paddingFraction;
      const latPad = (latRange === 0 ? 0.0001 : latRange) * paddingFraction;
      lonMin -= lonPad; lonMax += lonPad;
      latMin -= latPad; latMax += latPad;
    }

    const midLat = (latMin + latMax) / 2;
    const lonToLatFactor = Math.cos(midLat * Math.PI / 180);
    const geoW = (lonMax - lonMin) * lonToLatFactor;
    const geoH = latMax - latMin;
    const scale = Math.min(width / geoW, height / geoH);
    const xOff = (width  - geoW * scale) / 2;
    const yOff = (height - geoH * scale) / 2;
    const x = d3.scaleLinear().domain([lonMin, lonMax]).range([xOff, xOff + geoW * scale]);
    const y = d3.scaleLinear().domain([latMin, latMax]).range([yOff + geoH * scale, yOff]);

    const line = d3.line<LatLon | null>()
      .defined((d): d is LatLon => d !== null)
      .x((p) => x(p![1]))
      .y((p) => y(p![0]))
      .curve(d3.curveBasis);

    const segs: { d: string, color: string }[] = [];
    const colors = ['#1f77b4', '#ffba7d', '#9ef09e', '#f4afaf', '#c7aae2', '#93e4ef'];

    if (sectors.length > 0) {
      const sectorIndices = sectors.map(gate => {
        const midLatG = (gate[0][0] + gate[1][0]) / 2;
        const midLonG = (gate[0][1] + gate[1][1]) / 2;

        let bestIdx = 0;
        let minD = Infinity;
        for (let i = 0; i < points.length; i++) {
          const p = points[i];
          if (!p) continue;
          const d = (p[0] - midLatG) ** 2 + (p[1] - midLonG) ** 2;
          if (d < minD) { minD = d; bestIdx = i; }
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
      const lineValid = d3.line<LatLon>()
        .x((p: LatLon) => x(p[1]))
        .y((p: LatLon) => y(p[0]))
        .curve(d3.curveBasis);

      const runs: LatLon[][] = [];
      let cur: LatLon[] = [];
      for (const p of points) {
        if (p !== null) { cur.push(p); }
        else if (cur.length > 0) { runs.push(cur); cur = []; }
      }
      if (cur.length > 0) runs.push(cur);

      const smooth = (pts: LatLon[], half: number): LatLon[] =>
        pts.map((_, i) => {
          const lo = Math.max(0, i - half);
          const hi = Math.min(pts.length - 1, i + half);
          let sLat = 0, sLon = 0;
          for (let j = lo; j <= hi; j++) { sLat += pts[j][0]; sLon += pts[j][1]; }
          const n = hi - lo + 1;
          return [sLat / n, sLon / n] as LatLon;
        });

      for (let pts of runs) {
        pts = smooth(pts, 20);
        pts = smooth(pts, 12);
        pts = smooth(pts, 6);
        if (pts.length >= 2) segs.push({ d: lineValid(pts) || '', color: '#3b82f6' });
      }
    }

    return { xScale: x, yScale: y, segments: segs };
  }, [points, sectors, width, height]);

  const getPointScreenInfo = useCallback((index: number) => {
    const pt = points[index];
    if (!pt || !xScale || !yScale) return null;

    const x = xScale(pt[1]);
    const y = yScale(pt[0]);

    let prevPt: LatLon | null = null;
    for (let i = index - 1; i >= 0; i--) {
      if (points[i] !== null) { prevPt = points[i] as LatLon; break; }
    }
    let nextPt: LatLon | null = null;
    for (let i = index + 1; i < points.length; i++) {
      if (points[i] !== null) { nextPt = points[i] as LatLon; break; }
    }

    if (!prevPt && !nextPt) return { x, y, angle: 0 };
    if (!prevPt) prevPt = pt;
    if (!nextPt) nextPt = pt;
    if (prevPt === nextPt) return { x, y, angle: 0 };

    const pPrev = { x: xScale(prevPt[1]), y: yScale(prevPt[0]) };
    const pNext = { x: xScale(nextPt[1]), y: yScale(nextPt[0]) };

    const dx = pNext.x - pPrev.x;
    const dy = pNext.y - pPrev.y;
    const angle = Math.atan2(dy, dx) + Math.PI / 2;

    return { x, y, angle };
  }, [points, xScale, yScale]);



  useEffect(() => {
    if (!needsAutoGateRef.current || points.length === 0 || !xScale || !yScale) return;
    needsAutoGateRef.current = false;

    const firstIdx = points.findIndex((p): p is LatLon => p !== null);
    if (firstIdx === -1) return;

    const info = getPointScreenInfo(firstIdx);
    if (!info) return;

    const len = 50;
    const dx = (len / 2) * Math.cos(info.angle);
    const dy = (len / 2) * Math.sin(info.angle);
    const p1: LatLon = [yScale.invert(info.y - dy), xScale.invert(info.x - dx)];
    const p2: LatLon = [yScale.invert(info.y + dy), xScale.invert(info.x + dx)];
    setSectors([[p1, p2]]);
  }, [points, xScale, yScale, getPointScreenInfo]);

  const handleClearPoints = () => {
    if (confirm('Clear all saved track points? This will also clear any defined sectors.')) {
      setPoints([]);
      setPointCount(0);
      countRef.current = 0;
      lastPointRef.current = null;
      setSectors([]);
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
      const firstIdx = points.findIndex((p): p is LatLon => p !== null);
      if (firstIdx !== -1) {
        const info = getPointScreenInfo(firstIdx);
        if (info) {
          const length = 60;
          const dx = (length / 2) * Math.cos(info.angle);
          const dy = (length / 2) * Math.sin(info.angle);

          const p1: LatLon = [yScale.invert(info.y - dy), xScale.invert(info.x - dx)];
          const p2: LatLon = [yScale.invert(info.y + dy), xScale.invert(info.x + dx)];
          setSectors([[p1, p2]]);
        }
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
      const gateCoords: SectorGate = [p1, p2];

      setSectors((prev) => {
        const rest = prev.slice(1);
        return [gateCoords, ...rest];
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
          points: points.filter((p): p is LatLon => p !== null),
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

  const handleCSVFile = (file: File) => {
    setIsLoadingCSV(true);
    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const text = e.target?.result as string;
        const lines = text.split(/\r?\n/).filter(Boolean);
        if (lines.length < 2) { alert('CSV has no data rows.'); return; }

        const headers = parseCSVLine(lines[0]);
        const gpsIdx = headers.indexOf('dynamics.gps');
        if (gpsIdx === -1) { alert('No "dynamics.gps" column found in this CSV.'); return; }

        const extracted: (LatLon | null)[] = [];
        let prevWasValid = false;
        for (let i = 1; i < lines.length; i++) {
          const fields = parseCSVLine(lines[i]);
          const cell = fields[gpsIdx]?.trim();
          if (!cell || cell === '[]') {
            if (prevWasValid) { extracted.push(null); prevWasValid = false; }
            continue;
          }
          const match = cell.match(/\[?\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\]?/);
          if (!match) {
            if (prevWasValid) { extracted.push(null); prevWasValid = false; }
            continue;
          }
          const lat = parseFloat(match[1]);
          const lon = parseFloat(match[2]);
          if (isFinite(lat) && isFinite(lon)) {
            extracted.push([lat, lon]);
            prevWasValid = true;
          } else {
            if (prevWasValid) { extracted.push(null); prevWasValid = false; }
          }
        }

        const validCount = extracted.filter(p => p !== null).length;
        if (validCount === 0) { alert('No valid GPS coordinates found in this CSV.'); return; }

        const validPoints = points.filter(p => p !== null);
        if (validPoints.length > 0 && !confirm(`Replace existing ${validPoints.length} track points with ${validCount} points from CSV?`)) return;

        setPoints(extracted);
        setSectors([]);
        setIsRecording(false);
        setPointCount(validCount);
        countRef.current = validCount;
        lastPointRef.current = [...extracted].reverse().find((p): p is LatLon => p !== null) ?? null;

        const name = file.name.replace(/\.csv$/i, '');
        if (trackName === 'Untitled') setTrackName(name);
      } finally {
        setIsLoadingCSV(false);
        if (csvInputRef.current) csvInputRef.current.value = '';
      }
    };
    reader.onerror = () => { alert('Failed to read file.'); setIsLoadingCSV(false); };
    reader.readAsText(file);
  };

  const handleMouseMove = useCallback((event: React.MouseEvent<SVGSVGElement>) => {
    if ((!isDefiningSectors && !isSettingStartGate) || !xScale || !yScale) return;

    const svg = event.currentTarget;
    const pt = svg.createSVGPoint();
    pt.x = event.clientX;
    pt.y = event.clientY;
    const svgPoint = pt.matrixTransform(svg.getScreenCTM()?.inverse());

    let minD = Infinity;
    let bestIdx = -1;

    for (let i = 0; i < points.length; i++) {
      const p = points[i];
      if (!p) continue;
      const px = xScale(p[1]);
      const py = yScale(p[0]);
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

  const handleSvgClick = useCallback((_event: React.MouseEvent<SVGSVGElement>) => {
    if (!isDefiningSectors || !xScale || !yScale) return;

    if (hoverLine) {
      const currentIndex = hoverLine.index;
      const isTooClose = sectors.some(gate => {
        const midLatG = (gate[0][0] + gate[1][0]) / 2;
        const midLonG = (gate[0][1] + gate[1][1]) / 2;

        let bestIdx = 0;
        let minD = Infinity;
        for (let i = 0; i < points.length; i++) {
          const p = points[i];
          if (!p) continue;
          const d = (p[0] - midLatG) ** 2 + (p[1] - midLonG) ** 2;
          if (d < minD) { minD = d; bestIdx = i; }
        }
        return Math.abs(bestIdx - currentIndex) < 10;
      });

      if (isTooClose) return;

      const length = 60;
      const dx = (length / 2) * Math.cos(hoverLine.angle);
      const dy = (length / 2) * Math.sin(hoverLine.angle);

      const p1: LatLon = [yScale.invert(hoverLine.y - dy), xScale.invert(hoverLine.x - dx)];
      const p2: LatLon = [yScale.invert(hoverLine.y + dy), xScale.invert(hoverLine.x + dx)];

      setSectors((prev: SectorGate[]) => [...prev, [p1, p2]]);
    }
  }, [isDefiningSectors, hoverLine, xScale, yScale, sectors, points]);

  return (
    <div className="h-full w-full flex flex-col bg-white">
      {/* Compact status bar */}
      <div className="px-3 py-2 flex items-center gap-3 border-b bg-gray-50 shrink-0">
        <div className={`w-2.5 h-2.5 rounded-full shrink-0 transition-colors ${isReceivingData ? 'bg-green-500' : 'bg-gray-400'}`} />
        <div className="text-xs font-medium text-gray-700 truncate">
          {isReceivingData
            ? (isRecording ? `Live ${selectedCarLabel} · Recording` : `Live ${selectedCarLabel} · View Only`)
            : 'Disconnected'}
        </div>
        <div className="text-xs text-gray-400 shrink-0">{pointCount} pts · {sectors.length} sectors</div>
        {!isFullscreen && trackName && (
          <div className="text-xs text-gray-500 truncate ml-auto">{trackName}</div>
        )}
        <button
          onClick={() => csvInputRef.current?.click()}
          disabled={isLoadingCSV}
          title="Upload CSV"
          className="ml-auto shrink-0 text-gray-400 hover:text-blue-500 disabled:text-gray-300 disabled:cursor-not-allowed"
        >
          <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>
          </svg>
        </button>
      </div>

      {/* Full controls — fullscreen only */}
      {isFullscreen && (
        <div className="px-3 py-2 flex flex-wrap items-center gap-2 border-b bg-gray-50 shrink-0">
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
          <button onClick={handleSaveAs} className="text-gray-400 hover:text-blue-500" title="Save As Copy">
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path><polyline points="17 21 17 13 7 13 7 21"></polyline><polyline points="7 3 7 8 15 8"></polyline></svg>
          </button>
          <button
            onClick={() => csvInputRef.current?.click()}
            disabled={isLoadingCSV}
            className="px-3 py-1 text-xs bg-blue-50 text-blue-700 border border-blue-300 rounded hover:bg-blue-100 disabled:opacity-50 disabled:cursor-not-allowed transition"
          >
            {isLoadingCSV ? 'Loading…' : 'Upload CSV'}
          </button>
          <div className="text-xs text-gray-500 border-l pl-2 ml-1">Last: <span className="font-mono">{lastPointText}</span></div>
          <div className="flex flex-wrap items-center gap-2 border-l pl-2 ml-1">
            {isRecording && points.length > 0 && (
              <button onClick={handleFinishMapping} className="px-3 py-1 text-xs bg-indigo-500 text-white rounded hover:bg-indigo-600 transition">
                Finish Mapping
              </button>
            )}
            <button
              onClick={handleToggleSetStartGate}
              disabled={pointCount === 0}
              className={`px-3 py-1 text-xs rounded transition font-semibold border-2 ${
                isSettingStartGate
                  ? 'bg-green-600 text-white border-green-700 hover:bg-green-700'
                  : sectors[0]
                  ? 'bg-green-100 text-green-800 border-green-400 hover:bg-green-200'
                  : 'bg-orange-500 text-white border-orange-600 hover:bg-orange-600 animate-pulse'
              } disabled:bg-gray-300 disabled:border-gray-300 disabled:cursor-not-allowed disabled:animate-none`}
            >
              {isSettingStartGate ? 'Click track to place…' : sectors[0] ? 'Start Gate ✓' : 'Set Start Gate *'}
            </button>
            <button
              onClick={handleToggleDefineSectors}
              disabled={pointCount === 0}
              className={`px-3 py-1 text-xs rounded transition ${isDefiningSectors ? 'bg-green-500 text-white hover:bg-green-600' : 'bg-blue-500 text-white hover:bg-blue-600'} disabled:bg-gray-300 disabled:cursor-not-allowed`}
            >
              {isDefiningSectors ? 'Finish' : 'Define Sectors'}
            </button>
            <button onClick={handleClearSectors} disabled={sectors.length === 0} className="px-3 py-1 text-xs bg-yellow-500 text-white rounded hover:bg-yellow-600 disabled:bg-gray-300 disabled:cursor-not-allowed transition">
              Clear Sectors
            </button>
            <button onClick={handleClearPoints} disabled={pointCount === 0} className="px-3 py-1 text-xs bg-red-500 text-white rounded hover:bg-red-600 disabled:bg-gray-300 disabled:cursor-not-allowed transition">
              Clear Track
            </button>
            <div className="flex flex-col items-end gap-1">
              <button
                onClick={handleSaveToDatabase}
                disabled={!sectors[0] || isSavingToDb || pointCount === 0}
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
      )}

      {/* SVG visualization area */}
      <div className="flex-grow overflow-hidden relative">
        {pointCount > 0 ? (
          <>
            <svg
              viewBox={`0 0 ${width} ${height}`}
              className={`w-full h-full ${(isDefiningSectors || isSettingStartGate) ? 'cursor-crosshair' : ''}`}
              preserveAspectRatio="xMidYMid meet"
              onClick={isSettingStartGate ? handleSvgClickStartGate : handleSvgClick}
              onMouseMove={handleMouseMove}
              onMouseLeave={handleMouseLeave}
            >
              {segments.map((seg, i) => (
                <path
                  key={i}
                  d={seg.d}
                  stroke={(!isDefiningSectors && sectors.length > 0) ? seg.color : '#1f77b4'}
                  strokeWidth={8}
                  fill="none"
                  strokeLinecap="butt"
                  strokeLinejoin="round"
                  opacity={0.6}
                />
              ))}
              {sectors.map((gate, index) => {
                const x1 = xScale(gate[0][1]);
                const y1 = yScale(gate[0][0]);
                const x2 = xScale(gate[1][1]);
                const y2 = yScale(gate[1][0]);
                const isStartGate = index === 0;

                return (
                  <g key={`sector-${index}`}>
                    <line
                      x1={x1} y1={y1} x2={x2} y2={y2}
                      stroke={isStartGate ? '#16a34a' : 'red'}
                      strokeWidth={isStartGate ? 5 : 4}
                    />
                    <text
                      x={x2} y={y2} dx={5} dy={5}
                      fill={isStartGate ? '#16a34a' : 'red'}
                      fontSize="12" fontWeight="bold"
                      textAnchor="start" alignmentBaseline="hanging"
                    >{isStartGate ? 'START' : index}</text>
                  </g>
                );
              })}

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

      <input
        ref={csvInputRef}
        type="file"
        accept=".csv"
        className="hidden"
        onChange={(e) => { const f = e.target.files?.[0]; if (f) handleCSVFile(f); }}
      />

      {isFullscreen && sectors.length > 0 && (
        <div className="p-3 border-t bg-gray-50 text-xs overflow-y-auto max-h-40">
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
            {pointCount > 0 && lastValidPoint && (
              <div className="flex items-center gap-2 bg-white p-1 rounded border border-blue-300">
                <div className="h-4 px-1.5 rounded-full bg-blue-500 text-white flex items-center justify-center text-[10px] font-bold shrink-0">
                  Track End
                </div>
                <div className="flex flex-col font-mono text-gray-600">
                  <span>Lat: {lastValidPoint[0].toFixed(6)}</span>
                  <span>Lon: {lastValidPoint[1].toFixed(6)}</span>
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