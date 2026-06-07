'use client';
import './trackside.css';

import { useEffect, useMemo, useRef, useState, type ChangeEvent, type Dispatch, type MouseEvent, type ReactNode, type SetStateAction } from "react";
import { Activity, AlertTriangle, ArrowDown, ArrowUp, CalendarDays, ChevronLeft, ChevronRight, Disc3, Download, FileText, Flag, Gauge, GraduationCap, HelpCircle, MapPinned, Moon, NotebookText, Plus, Power, Radio, RefreshCcw, Save, Scissors, SlidersHorizontal, Sun, Target, Thermometer, Timer, Trash2, Upload, Users, WifiOff, X, Zap } from "lucide-react";
import { api } from "@/lib/trackside/api";
import { useCarStatus, CAR_STATE_META, humanizeReason, type CarState, type CarStatusFeed } from "@/lib/trackside/useCarStatus";
import { EVENT_TYPES, upsertSession, patchSession, syncRegistryWithServer, type TracksideSessionInfo } from "@/lib/trackside/sessionRegistry";
import { usePresence } from "@/lib/trackside/usePresence";
import { useSession } from "next-auth/react";
import { DashLayoutEditor } from "@/components/dashlayout/DashLayoutEditor";
import { DashMessageEditor } from "@/components/dashlayout/DashMessageEditor";
import type { LapCardLayout } from "@/lib/dash/dashLayout";
import { useDashSignals } from "@/lib/dash/dashSignals";
import { useMessageLibrary } from "@/lib/dash/useMessageLibrary";
import { activeMessages, MESSAGE_ICON_GLYPH, type DashMessage } from "@/lib/dash/dashMessages";
import { useRacePlan } from "@/lib/dash/useRacePlan";
import { DriveDaySetupForm, driveDaySetupToPayload, type DriveDaySetup } from "@/components/trackside/DriveDaySetupForm";
import type {
  ChannelDef,
  ChannelChartDefinition,
  DayDetail,
  DriveDay,
  GateLine,
  GpsPoint,
  KafkaTransport,
  LiveSample,
  LiveStreamEvent,
  SegmentSummary,
  SeriesPoint,
  SessionSummary,
  SourceDef,
  TrackDefinition,
} from "@/lib/trackside/types";

const NATURAL_COLLATOR = new Intl.Collator(undefined, { numeric: true, sensitivity: "base" });

const DEFAULT_TRACK: TrackDefinition = {
  name: "Orion Test Track",
  slug: "orion-test-track",
  notes: "",
  gates: [],
};

const DEFAULT_CHANNEL_CHART: ChannelChartDefinition = {
  name: "Orion Default",
  slug: "orion-default",
  notes: "Default Orion channel quantity/unit chart.",
  entries: [],
};
const ANGELIQUE_CHANNEL_CHART_SLUG = "angelique-channel-chart-04-14-26";
const DEFAULT_SOURCES: SourceDef[] = [
  { key: "orion", label: "Orion" },
  { key: "angelique", label: "Angelique" },
];
const VCU_MAX_MOTOR_RPM = 6000;
type VcuTorqueParamSet = {
  id: string;
  label: string;
  powerLimitTorque: number[];
  pedalMapX: number[];
  pedalMap: number[];
  pedalCurveExponent: number;
  lowCellDerateStartV: number;
  lowCellCutoffV: number;
  appsMinTravelDeadzone: number;
  appsMaxTravelDeadzone: number;
};
const VCU_DEFAULT_TORQUE_PARAMS: VcuTorqueParamSet = {
  id: "default",
  label: "Default",
  powerLimitTorque: [210, 210, 210, 210, 210, 192, 166, 145, 129, 115, 103],
  pedalMapX: [0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1],
  pedalMap: [0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1],
  pedalCurveExponent: 2,
  lowCellDerateStartV: 3.2,
  lowCellCutoffV: 2.8,
  appsMinTravelDeadzone: 0.09,
  appsMaxTravelDeadzone: 0.88,
};
const VCU_TORQUE_PARAM_SETS: VcuTorqueParamSet[] = [
  VCU_DEFAULT_TORQUE_PARAMS,
  { ...VCU_DEFAULT_TORQUE_PARAMS, id: "acceleration", label: "Acceleration" },
  { ...VCU_DEFAULT_TORQUE_PARAMS, id: "autocross", label: "Autocross" },
  { ...VCU_DEFAULT_TORQUE_PARAMS, id: "endurance", label: "Endurance" },
  {
    ...VCU_DEFAULT_TORQUE_PARAMS,
    id: "skidpad",
    label: "Skidpad",
    pedalMapX: [0, 0.04, 0.16, 0.28, 0.40, 0.52, 0.64, 0.70, 0.80, 0.90, 1],
    pedalMap: [0, 0.1904762, 0.2857143, 0.3809524, 0.4761905, 0.5714286, 0.6666667, 0.7142857, 0.7142857, 0.7142857, 0.7142857],
    pedalCurveExponent: 1,
  },
];

const METADATA_FIELDS = [
  "driver",
  "vehicle_id",
  "vehicle_weight",
  "vehicle_type",
  "vehicle_comment",
  "venue",
  "event",
  "session",
  "short_comment",
  "long_comment",
] as const;

type MetadataField = (typeof METADATA_FIELDS)[number];
type SessionMetadata = Record<MetadataField, string>;
type GateDrawMode = "start_finish" | "split" | null;
type AppTab = "exporter" | "live" | "track-builder" | "race-ops" | "dash";
type LapPreviewRow = {
  id: string;
  label: string;
  kind: "lap" | "outlap";
  startMs: number;
  endMs: number;
  durationMs: number;
};
type LiveLap = {
  id: string;
  label: string;
  kind: "flying" | "manual";
  startMs: number;
  endMs: number;
  durationMs: number;
  sectors: number[];
  energyWh: number;
  energyOutWh?: number;
  energyInWh?: number;
  distanceM: number;
  avgSpeedMps: number | null;
  samples: LiveSample[];
  notes?: string; // per-lap note (synced to the classifier DB)
};
type LiveSessionState = {
  running: boolean;
  connected: boolean;
  status: string;
  topic: string;
  startedAt: number | null;
  lastSample: LiveSample | null;
  previousSample: LiveSample | null;
  samples: LiveSample[];
  lapSamples: LiveSample[];
  laps: LiveLap[];
  lapStartMs: number | null;
  sectorStartMs: number | null;
  nextSplitIndex: number;
  currentSectors: number[];
  totalEnergyWh: number;
  totalEnergyOutWh: number;
  totalEnergyInWh: number;
  lapEnergyWh: number;
  lapEnergyOutWh: number;
  lapEnergyInWh: number;
  lapDistanceM: number;
  deltaRate: number | null;
  deltaMs: number;
};
type SavedCurrentLap = {
  lapStartMs: number | null;
  sectorStartMs: number | null;
  nextSplitIndex: number;
  currentSectors: number[];
  lapEnergyWh: number;
  lapEnergyOutWh?: number;
  lapEnergyInWh?: number;
  lapDistanceM: number;
  lapSamples: LiveSample[];
  deltaMs: number;
};
type SavedSession = {
  version: 1;
  savedAt: number;
  source: string;
  topic: string;
  targetLaps: number;
  targetEnergyKwh?: number;
  soeCutoffCellV?: number;
  totalEnergyWh: number;
  totalEnergyOutWh?: number;
  totalEnergyInWh?: number;
  laps: LiveLap[];
  selectedLapIds: string[];
  selectionSaved?: boolean;
  sampleTail: LiveSample[];
  currentLap?: SavedCurrentLap | null;
  hasSectors: boolean;
  sessionInfo?: TracksideSessionInfo | null;
};
const SESSION_STORAGE_KEY = "motec-live-session";
const TOUR_STORAGE_KEY = "trackside-tour-done";
const SESSION_AUTOSAVE_SAMPLE_CAP = 1500;
const LIVE_SAMPLE_MEMORY_CAP = 6000;
const LIVE_LAP_SAMPLE_MEMORY_CAP = 6000;
const SESSION_LOCAL_AUTOSAVE_MS = 500;
const SESSION_BACKEND_AUTOSAVE_MS = 2500;
// Only this browser's own fresh autosave is silently restored (so an accidental
// reload mid-session is seamless). Older or shared (backend) sessions are merely
// *offered* via the resume banner — never auto-applied — so a leftover/demo cache
// can't masquerade as a live session in production.
const SESSION_AUTO_RESTORE_MAX_AGE_MS = 12 * 60 * 60 * 1000;
const SESSION_RESUME_OFFER_MAX_AGE_MS = 24 * 60 * 60 * 1000;
const SESSION_IDB_NAME = "motec-live-session-cache";
const SESSION_IDB_STORE = "sessions";
type CarPreset = {
  id: string;
  name: string;
  source: "orion" | "angelique";
  topic: string;
  transport: KafkaTransport;
  trackSlug: string;
  channelChartSlug: string;
  metadata: SessionMetadata;
};

const EMPTY_METADATA: SessionMetadata = {
  driver: "",
  vehicle_id: "",
  vehicle_weight: "",
  vehicle_type: "EV",
  vehicle_comment: "",
  venue: "",
  event: "",
  session: "",
  short_comment: "",
  long_comment: "",
};

const EMPTY_LIVE_STATE: LiveSessionState = {
  running: false,
  connected: false,
  status: "Idle",
  topic: "",
  startedAt: null,
  lastSample: null,
  previousSample: null,
  samples: [],
  lapSamples: [],
  laps: [],
  lapStartMs: null,
  sectorStartMs: null,
  nextSplitIndex: 0,
  currentSectors: [],
  totalEnergyWh: 0,
  totalEnergyOutWh: 0,
  totalEnergyInWh: 0,
  lapEnergyWh: 0,
  lapEnergyOutWh: 0,
  lapEnergyInWh: 0,
  lapDistanceM: 0,
  deltaRate: null,
  deltaMs: 0,
};

function readLocalSavedSession() {
  try {
    const raw = localStorage.getItem(SESSION_STORAGE_KEY);
    return raw ? JSON.parse(raw) as SavedSession : null;
  } catch {
    return null;
  }
}

function writeLocalSavedSession(saved: SavedSession) {
  try {
    localStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(saved));
  } catch {
    // IndexedDB/backend cache still get a chance.
  }
}

function removeLocalSavedSession() {
  try {
    localStorage.removeItem(SESSION_STORAGE_KEY);
  } catch {
    // ignore
  }
}

function openSessionDb(): Promise<IDBDatabase | null> {
  if (!("indexedDB" in window)) return Promise.resolve(null);
  return new Promise((resolve) => {
    const request = indexedDB.open(SESSION_IDB_NAME, 1);
    request.onupgradeneeded = () => {
      request.result.createObjectStore(SESSION_IDB_STORE);
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => resolve(null);
    request.onblocked = () => resolve(null);
  });
}

async function writeIndexedSavedSession(saved: SavedSession) {
  const db = await openSessionDb();
  if (!db) return;
  await new Promise<void>((resolve) => {
    const tx = db.transaction(SESSION_IDB_STORE, "readwrite");
    tx.objectStore(SESSION_IDB_STORE).put(saved, "latest");
    tx.oncomplete = () => resolve();
    tx.onerror = () => resolve();
    tx.onabort = () => resolve();
  });
  db.close();
}

async function readIndexedSavedSession() {
  const db = await openSessionDb();
  if (!db) return null;
  const saved = await new Promise<SavedSession | null>((resolve) => {
    const tx = db.transaction(SESSION_IDB_STORE, "readonly");
    const request = tx.objectStore(SESSION_IDB_STORE).get("latest");
    request.onsuccess = () => resolve((request.result as SavedSession | undefined) ?? null);
    request.onerror = () => resolve(null);
    tx.onerror = () => resolve(null);
    tx.onabort = () => resolve(null);
  });
  db.close();
  return saved;
}

async function removeIndexedSavedSession() {
  const db = await openSessionDb();
  if (!db) return;
  await new Promise<void>((resolve) => {
    const tx = db.transaction(SESSION_IDB_STORE, "readwrite");
    tx.objectStore(SESSION_IDB_STORE).delete("latest");
    tx.oncomplete = () => resolve();
    tx.onerror = () => resolve();
    tx.onabort = () => resolve();
  });
  db.close();
}

function defaultCarPresets(): CarPreset[] {
  return [
    {
      id: "orion",
      name: "Orion",
      source: "orion",
      topic: "orion",
      transport: "mqtt",
      trackSlug: DEFAULT_TRACK.slug,
      channelChartSlug: DEFAULT_CHANNEL_CHART.slug,
      metadata: {
        ...EMPTY_METADATA,
        vehicle_id: "Orion",
        vehicle_type: "EV",
        event: "Orion Live Telemetry",
      },
    },
    {
      id: "angelique",
      name: "Angelique",
      source: "angelique",
      topic: "grafana_data_angelique",
      transport: "local",
      trackSlug: DEFAULT_TRACK.slug,
      channelChartSlug: ANGELIQUE_CHANNEL_CHART_SLUG,
      metadata: {
        ...EMPTY_METADATA,
        vehicle_id: "Angelique",
        vehicle_type: "EV",
        event: "Angelique Live Telemetry",
      },
    },
  ];
}

function loadCarPresets(): CarPreset[] {
  try {
    const parsed = JSON.parse(localStorage.getItem("motec-car-presets") || "null");
    if (!Array.isArray(parsed)) return defaultCarPresets();
    return parsed.map(normalizeCarPreset).filter(Boolean) as CarPreset[];
  } catch {
    return defaultCarPresets();
  }
}

function normalizeCarPreset(value: unknown): CarPreset | null {
  if (!value || typeof value !== "object") return null;
  const raw = value as Partial<CarPreset>;
  const source = raw.source === "angelique" ? "angelique" : "orion";
  const name = String(raw.name || (source === "angelique" ? "Angelique" : "Orion")).trim();
  const metadata = { ...EMPTY_METADATA, ...(raw.metadata || {}) };
  const channelChartSlug = String(raw.channelChartSlug || defaultChannelChartSlugForSource(source));
  const migratedChannelChartSlug =
    source === "angelique" && raw.id === "angelique" && channelChartSlug === DEFAULT_CHANNEL_CHART.slug
      ? ANGELIQUE_CHANNEL_CHART_SLUG
      : channelChartSlug;
  return {
    id: String(raw.id || slugifyTrackName(name) || source),
    name,
    source,
    topic: String(raw.topic || (source === "orion" ? "orion" : `grafana_data_${source}`)),
    transport: raw.transport === "mqtt" ? "mqtt" : raw.transport === "kafka" ? "kafka" : "local",
    trackSlug: String(raw.trackSlug || DEFAULT_TRACK.slug),
    channelChartSlug: migratedChannelChartSlug,
    metadata,
  };
}

function App() {
  const [activeTab, setActiveTab] = useState<AppTab>("live");
  // First-load onboarding tour. Opens automatically until the user finishes/skips
  // it once (persisted in localStorage); re-openable any time via the help button.
  const [showTour, setShowTour] = useState(false);
  useEffect(() => {
    try {
      if (localStorage.getItem(TOUR_STORAGE_KEY) !== "done") setShowTour(true);
    } catch {
      setShowTour(true);
    }
  }, []);
  const dismissTour = (markDone: boolean) => {
    setShowTour(false);
    if (markDone) {
      try {
        localStorage.setItem(TOUR_STORAGE_KEY, "done");
      } catch {
        // ignore storage errors (private mode) — tour just reopens next load
      }
    }
  };
  const [health, setHealth] = useState<{ source: string; postgres_enabled: boolean } | null>(null);
  const [sources, setSources] = useState<SourceDef[]>(DEFAULT_SOURCES);
  const [source, setSource] = useState<"orion" | "angelique">("orion");
  const [channels, setChannels] = useState<ChannelDef[]>([]);
  const [channel, setChannel] = useState("");
  const [threshold, setThreshold] = useState(0);
  const [minDurationS, setMinDurationS] = useState(10);
  const [exportType, setExportType] = useState<"motec" | "csv">("motec");
  const [days, setDays] = useState<DriveDay[]>([]);
  const [sessionCountsByDate, setSessionCountsByDate] = useState<Record<string, number>>({});
  const [selectedDate, setSelectedDate] = useState("");
  const [detail, setDetail] = useState<DayDetail | null>(null);
  const [session, setSession] = useState<SessionSummary | null>(null);
  const [segments, setSegments] = useState<SegmentSummary[]>([]);
  // Track Builder only uses sessions with a real GPS trace (you draw gates on
  // it), so hide non-GPS / sparse-trace sessions by default — that's what filters
  // out stale/replayed "fake" sessions without touching the database. Toggle off
  // to see everything. MIN_TRACE_GPS_POINTS = a real lap has many fixes.
  const [gpsOnlySessions, setGpsOnlySessions] = useState(true);
  const [series, setSeries] = useState<SeriesPoint[]>([]);
  const [gps, setGps] = useState<GpsPoint[]>([]);
  const [range, setRange] = useState<[number, number] | null>(null);
  const [tracks, setTracks] = useState<TrackDefinition[]>([]);
  const [track, setTrack] = useState<TrackDefinition>(DEFAULT_TRACK);
  const [channelCharts, setChannelCharts] = useState<ChannelChartDefinition[]>([]);
  const [channelChart, setChannelChart] = useState<ChannelChartDefinition>(DEFAULT_CHANNEL_CHART);
  const [gateDrawMode, setGateDrawMode] = useState<GateDrawMode>(null);
  const [selectedSegments, setSelectedSegments] = useState<Set<string>>(new Set());
  const [previewSelectedSegments, setPreviewSelectedSegments] = useState<Set<string>>(new Set());
  const [lastPreviewSegmentId, setLastPreviewSegmentId] = useState("");
  const [exportUrl, setExportUrl] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [sessionMetadata, setSessionMetadata] = useState<Record<string, SessionMetadata>>({});
  const [metadataDraft, setMetadataDraft] = useState<SessionMetadata>(EMPTY_METADATA);
  // The active named session (for the logsync registry + export identity).
  const [sessionInfo, setSessionInfo] = useState<TracksideSessionInfo | null>(null);
  const sessionInfoRef = useRef(sessionInfo);
  sessionInfoRef.current = sessionInfo;
  const lapSyncTimerRef = useRef<number | null>(null);
  const [newSessionOpen, setNewSessionOpen] = useState(false);
  const [lapDesignerOpen, setLapDesignerOpen] = useState(false);
  const [layoutSendStatus, setLayoutSendStatus] = useState("");
  const [mixedMetadata, setMixedMetadata] = useState<Set<MetadataField>>(new Set());
  const [darkMode, setDarkMode] = useState(() => localStorage.getItem("motec-theme") === "dark");
  const [builderCenter, setBuilderCenter] = useState({ lat: 30.3922, lon: -97.7287 });
  const [builderSearch, setBuilderSearch] = useState("30.3922, -97.7287");
  const [autoLiveDownload, setAutoLiveDownload] = useState(false);
  // Manual MoTeC recording: when recording, recordingStartMs is the window start.
  const [recordingStartMs, setRecordingStartMs] = useState<number | null>(null);
  const [recordingLabel, setRecordingLabel] = useState("");
  const [recordingBusy, setRecordingBusy] = useState(false);
  const [energyWindowS, setEnergyWindowS] = useState(60);
  const [targetLaps, setTargetLaps] = useState(() => Number(localStorage.getItem("motec-target-laps") || 0));
  const [targetEnergyKwh, setTargetEnergyKwh] = useState(() => Number(localStorage.getItem("motec-target-energy-kwh") || PACK_ENERGY_KWH));
  const [soeCutoffCellV, setSoeCutoffCellV] = useState(loadSoeCutoffCellV);
  const [selectedLapIds, setSelectedLapIds] = useState<Set<string>>(() => new Set());
  const [sessionFromFile, setSessionFromFile] = useState(false);
  const [resumeAvailable, setResumeAvailable] = useState<SavedSession | null>(null);
  // True after Stop Event until the next session/feed (re)start, so the UI shows
  // a clearly-ended state instead of looking like a lap is still running.
  const [eventEnded, setEventEnded] = useState(false);
  const knownLapIdsRef = useRef<Set<string>>(new Set());
  const lastLocalSessionSaveRef = useRef(0);
  const lastBackendSessionSaveRef = useRef(0);
  const lastSavedLapCountRef = useRef(0);
  const sessionFileInputRef = useRef<HTMLInputElement | null>(null);
  const [liveSampleHz, setLiveSampleHz] = useState(() => Number(localStorage.getItem("motec-live-sample-hz") || 2));
  const [torqueParamSetId, setTorqueParamSetId] = useState(() => localStorage.getItem("motec-vcu-torque-param-set") || "skidpad");
  const [liveTopic, setLiveTopic] = useState(() => localStorage.getItem("motec-live-topic") || "");
  const [liveTransport, setLiveTransport] = useState<KafkaTransport>(() => {
    const stored = localStorage.getItem("motec-live-transport");
    return stored === "mqtt" || stored === "kafka" ? stored : "local";
  });
  const [carPresets, setCarPresets] = useState<CarPreset[]>(loadCarPresets);
  const [selectedCarPresetId, setSelectedCarPresetId] = useState(() => localStorage.getItem("motec-selected-car-preset") || "orion");
  const [carPresetDraft, setCarPresetDraft] = useState<CarPreset>(() => loadCarPresets()[0] ?? defaultCarPresets()[0]);
  const [liveState, setLiveState] = useState<LiveSessionState>(EMPTY_LIVE_STATE);
  // Dash pacing signals (publishes targetPower + lapTrigger to the on-car dash).
  const dashSignals = useDashSignals();
  const [dashTargetPower, setDashTargetPower] = useState<number>(() => Number(localStorage.getItem("dash-target-power") || 30));
  // Power-budget mode: 'auto' derives kW from the energy plan; 'manual' uses the slider.
  const [dashPowerMode, setDashPowerMode] = useState<"auto" | "manual">(() => (localStorage.getItem("dash-power-mode") === "manual" ? "manual" : "auto"));
  const [dashAutoLap, setDashAutoLap] = useState(() => localStorage.getItem("dash-auto-lap") === "1");
  const [dashGatePushed, setDashGatePushed] = useState(false);
  // How long the on-car full-screen lap card stays up after a lap (seconds).
  const [lapCardDurationS, setLapCardDurationS] = useState(() => {
    const v = Number(localStorage.getItem("dash-lap-card-duration-s"));
    return Number.isFinite(v) && v > 0 ? v : 5;
  });
  // When on, the event-flag buttons (Cone / Off-track / Incomplete / custom)
  // also flash a message on the driver's dash.
  const [flagSendsMessage, setFlagSendsMessage] = useState(() => localStorage.getItem("flag-sends-message") === "1");
  // Per-flag choice of WHICH dash screen fires. Maps flag label → message-library
  // id, or the sentinels "auto" (build one from the flag text) / "none" (skip).
  const [flagMessageMap, setFlagMessageMap] = useState<Record<string, string>>(() => {
    try { return JSON.parse(localStorage.getItem("flag-message-map") || "{}") as Record<string, string>; }
    catch { return {}; }
  });
  // Driver-message palette (quick-send nudges to the dash) — shared server-side.
  const msgLib = useMessageLibrary();
  const [msgEditorOpen, setMsgEditorOpen] = useState(false);
  // Drive-day setup (conditions/tires/aero/alignment) — carried on the session,
  // synced, and written to the drive_day record.
  const [driveDaySetup, setDriveDaySetup] = useState<DriveDaySetup>({});
  const [setupOpen, setSetupOpen] = useState(false);
  const [flagScreensOpen, setFlagScreensOpen] = useState(false);
  const [confirmStopOpen, setConfirmStopOpen] = useState(false);
  // Per-page-load dismissal of the "you started this session — reclaim?" banner.
  // Resets on reload; the user can always re-take by reloading the page.
  const [starterReclaimDismissed, setStarterReclaimDismissed] = useState(false);
  const driveDaySetupRef = useRef(driveDaySetup);
  driveDaySetupRef.current = driveDaySetup;
  const [msgSendStatus, setMsgSendStatus] = useState("");
  const [dashNow, setDashNow] = useState(0); // 1 Hz tick for link-health "age" displays
  const dashLastLapCountRef = useRef(0);
  const lastRegistryPatchRef = useRef(0);
  // Multi-client sync: one leader owns the session, others mirror it read-only.
  const presence = usePresence(true);
  const isMirror = presence.isMirror;
  const { data: authSession } = useSession();
  const isAdmin = !!authSession?.user?.isAdmin;
  const isMirrorRef = useRef(false);
  const lastMirrorPollRef = useRef(0);
  const liveSourceRef = useRef<EventSource | null>(null);
  // The live feed auto-starts and self-heals: liveShouldRunRef tracks whether we
  // *want* a connection (so an unexpected drop reconnects, but an explicit
  // stop/reset does not), and reconnectTimerRef holds the pending retry.
  const liveShouldRunRef = useRef(false);
  const reconnectTimerRef = useRef<number | null>(null);
  const liveStateRef = useRef<LiveSessionState>(EMPTY_LIVE_STATE);
  const liveTrackRef = useRef(track);
  const liveMetadataRef = useRef(metadataDraft);
  const liveChannelChartRef = useRef(channelChart);
  const liveAutoDownloadRef = useRef(autoLiveDownload);
  const liveSourceNameRef = useRef(source);

  const selectedChannel = channels.find((c) => c.key === channel);
  // Numeric-aware ordering for the pickers so repeated/array channels (e.g.
  // pack.cells_v[0..131]) list as 0,1,2,…,10,…,100 instead of lexicographically
  // (the schema-driven natural-sort idea from the loggerd CSV work, PR #284).
  const sortedChannels = useMemo(
    () => [...channels].sort((a, b) => NATURAL_COLLATOR.compare(a.label, b.label)),
    [channels],
  );
  const sourceLabel = sources.find((item) => item.key === source)?.label || source;
  const hasStartFinish = track.gates.some((gate) => gate.role === "start_finish");
  const splitGates = track.gates.filter((gate) => gate.role === "split");
  // Incremental smoothing cache: smoothLiveSample is a causal filter (output
  // depends only on the raw sample + previous smoothed), so each raw sample is
  // smoothed exactly once and memoized by object identity. This turns the
  // per-frame cost from O(buffer) full re-smooths into one real smooth per new
  // sample, keeping the UI fluid even at high sample rates.
  const smoothCacheRef = useRef(new WeakMap<LiveSample, LiveSample>());
  const filteredLiveSamples = useMemo(() => {
    const src = liveState.samples;
    const cache = smoothCacheRef.current;
    const out = new Array<LiveSample>(src.length);
    let prev: LiveSample | null = null;
    for (let i = 0; i < src.length; i++) {
      const raw = src[i];
      let s = cache.get(raw);
      if (s === undefined) {
        s = smoothLiveSample(raw, prev);
        cache.set(raw, s);
      }
      out[i] = s;
      prev = s;
    }
    return out;
  }, [liveState.samples]);
  const filteredLiveLastSample = filteredLiveSamples.at(-1) ?? liveState.lastSample;
  const filteredLiveState = useMemo<LiveSessionState>(() => ({
    ...liveState,
    lastSample: filteredLiveLastSample,
    previousSample: filteredLiveSamples.length > 1 ? filteredLiveSamples[filteredLiveSamples.length - 2] : liveState.previousSample,
    samples: filteredLiveSamples,
  }), [liveState, filteredLiveLastSample, filteredLiveSamples]);
  const bestLap = useMemo(() => liveState.laps.reduce<LiveLap | null>((best, lap) => (!best || lap.durationMs < best.durationMs ? lap : best), null), [liveState.laps]);
  const bestSectors = useMemo(() => bestSectorTimes(liveState.laps), [liveState.laps]);
  const torqueParamSet = useMemo(
    () => VCU_TORQUE_PARAM_SETS.find((item) => item.id === torqueParamSetId) ?? VCU_TORQUE_PARAM_SETS[0],
    [torqueParamSetId],
  );
  const targetEnergyPerLapWh = targetLaps > 0 && targetEnergyKwh > 0 ? (targetEnergyKwh * 1000) / targetLaps : null;
  // --- dynamic per-lap energy budget (Wh), recomputed each completed lap ---
  const energyUsedWh = useMemo(
    () => liveState.laps.reduce((sum, lap) => sum + (lap.energyWh || 0), 0),
    [liveState.laps],
  );
  const lapsCompletedPlan = liveState.laps.length;
  const totalBudgetWh = targetEnergyKwh > 0 ? targetEnergyKwh * 1000 : null;
  const remainingBudgetWh = totalBudgetWh != null ? totalBudgetWh - energyUsedWh : null;
  const lapsRemainingPlan = targetLaps > 0 ? Math.max(0, targetLaps - lapsCompletedPlan) : null;
  // Headline budget: how much you can spend on EACH remaining lap from here.
  // Over-use shrinks remaining Wh → this drops; under-use grows it → this rises.
  const dynamicLapBudgetWh = remainingBudgetWh != null && lapsRemainingPlan != null && lapsRemainingPlan > 0
    ? remainingBudgetWh / lapsRemainingPlan : null;
  // + = banked margin vs the even split; − = over budget, must conserve.
  const budgetVsTargetWh = dynamicLapBudgetWh != null && targetEnergyPerLapWh != null
    ? dynamicLapBudgetWh - targetEnergyPerLapWh : null;
  // Auto-derive the dash target power from the energy plan: the live Wh/lap
  // budget ÷ the rolling average lap time is the power rate that spends the
  // budget evenly. A manual override (dashPowerMode='manual') wins when set.
  const avgLapTimeS = useMemo(() => {
    const recent = liveState.laps.slice(-3);
    if (!recent.length) return null;
    return recent.reduce((s, l) => s + l.durationMs, 0) / recent.length / 1000;
  }, [liveState.laps]);
  const autoTargetPowerKw = dynamicLapBudgetWh != null && avgLapTimeS != null && avgLapTimeS > 0
    ? dynamicLapBudgetWh / (avgLapTimeS / 3600) / 1000 : null;
  const effectiveTargetPowerKw = dashPowerMode === "manual" ? dashTargetPower : autoTargetPowerKw;

  // The race plan (total laps + usable budget + OCV cutoff) syncs across every
  // trackside client through THIS dedicated channel only — it's the single
  // source of truth. The shared saved-session doc must NOT also apply these
  // fields (it lags behind edits and would fight this poll, causing the value
  // to flip between the fresh and stale numbers).
  const racePlan = useRacePlan((p) => {
    if (p.totalLaps !== targetLaps) setTargetLaps(p.totalLaps);
    if (p.budgetKwh > 0 && p.budgetKwh !== targetEnergyKwh) setTargetEnergyKwh(p.budgetKwh);
    if (p.soeCutoffCellV && p.soeCutoffCellV > 0 && p.soeCutoffCellV !== soeCutoffCellV) {
      setSoeCutoffCellV(normalizeSoeCutoffCellV(p.soeCutoffCellV));
    }
  });
  const selectedLaps = useMemo(() => liveState.laps.filter((lap) => selectedLapIds.has(lap.id)), [liveState.laps, selectedLapIds]);
  const lapAverages = useMemo(() => {
    if (!selectedLaps.length) {
      return {
        count: 0,
        avgMs: null as number | null,
        avgEnergyWh: null as number | null,
        avgEnergyOutWh: null as number | null,
        avgEnergyInWh: null as number | null,
      };
    }
    const avgMs = selectedLaps.reduce((sum, lap) => sum + lap.durationMs, 0) / selectedLaps.length;
    const avgEnergyWh = selectedLaps.reduce((sum, lap) => sum + lap.energyWh, 0) / selectedLaps.length;
    const avgEnergyOutWh = selectedLaps.reduce((sum, lap) => sum + lapEnergyOutWh(lap), 0) / selectedLaps.length;
    const avgEnergyInWh = selectedLaps.reduce((sum, lap) => sum + lapEnergyInWh(lap), 0) / selectedLaps.length;
    return { count: selectedLaps.length, avgMs, avgEnergyWh, avgEnergyOutWh, avgEnergyInWh };
  }, [selectedLaps]);
  // Hero "Lap" tile reads COMPLETED laps, not click count. A lap is counted
  // when its end-of-lap click closes it (or when GPS auto-detect crosses S/F).
  // The lap currently in progress is implied by the running timer above, not
  // by the counter — so 4/22 means "4 done, on the 5th".
  const currentLapNumber = liveState.laps.length;
  const liveSectorCount = sessionFromFile && hasStartFinish && splitGates.length ? splitGates.length + 1 : 0;
  const liveLapElapsedMs = liveState.lastSample && liveState.lapStartMs ? Math.max(0, liveState.lastSample.t - liveState.lapStartMs) : 0;
  // Wall-clock ticker for the manual recording timer (runs only while recording).
  const [recordingNowMs, setRecordingNowMs] = useState(0);
  useEffect(() => {
    if (recordingStartMs == null) return;
    setRecordingNowMs(Date.now());
    const id = window.setInterval(() => setRecordingNowMs(Date.now()), 100);
    return () => window.clearInterval(id);
  }, [recordingStartMs]);
  const recordingElapsedMs = recordingStartMs == null ? 0 : Math.max(0, recordingNowMs - recordingStartMs);
  const liveTorqueNm = torqueFeedbackNmFor(filteredLiveState.lastSample);
  // Driver inputs for the hero (throttle %, front brake psi, steering °).
  const heroControls = useMemo(() => driverControls(filteredLiveState.lastSample, torqueParamSet), [filteredLiveState.lastSample, torqueParamSet]);
  const liveBadgeLabel = liveState.lastSample ? "Live" : liveState.connected ? "Listening" : liveState.running ? "Connecting" : "Standby";
  const liveBadgeClass = liveState.lastSample ? "liveBadge liveOn" : liveState.connected ? "liveBadge liveListening" : liveState.running ? "liveBadge liveConnecting" : "liveBadge";

  // ── Real liveness ───────────────────────────────────────────────────────────
  // Wall-clock arrival time of the last live *sample* (not heartbeats). This is
  // the truthful "telemetry is flowing right now" signal, independent of the
  // sample's own timestamp (which is replay-time for the simulator). Updated
  // from handleLiveEvent on every "sample" event.
  const [lastDataAtMs, setLastDataAtMs] = useState<number | null>(null);
  const [liveNowMs, setLiveNowMs] = useState(() => Date.now());
  // Tick once a second unconditionally so the data-age stays accurate even if
  // the local `running` flag briefly flips (e.g. a mirror mid-adopt) while real
  // samples are still arriving.
  useEffect(() => {
    setLiveNowMs(Date.now());
    const id = window.setInterval(() => setLiveNowMs(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, []);
  const dataAgeMs = lastDataAtMs == null ? null : Math.max(0, liveNowMs - lastDataAtMs);
  // Time spent trying to get telemetry since the feed started — used to escalate
  // 'waiting' to a hard 'down' after a grace period so a car-off page-load
  // doesn't sit on yellow forever just because the broker is reachable.
  const waitingForMs = lastDataAtMs == null && liveState.startedAt != null
    ? Math.max(0, liveNowMs - liveState.startedAt) : 0;
  // Telemetry (sample) freshness drives the badge. Fresh samples mean the car
  // is online RIGHT NOW, so freshness wins over the local `running`/connection
  // flags — those can momentarily desync on a mirror that's mid-adopt, which
  // used to flip the badge to "Offline" even while gauges kept updating. We
  // only fall back to the connection state when no data is actually flowing.
  const uplink: "idle" | "connecting" | "waiting" | "live" | "stale" | "down" =
    dataAgeMs != null && dataAgeMs < 3500
      ? "live"
      : dataAgeMs != null && dataAgeMs < 12000
        ? "stale"
        : dataAgeMs != null
          ? (liveState.running ? "down" : "idle") // had data, now silent
          : !liveState.running
            ? "idle"
            : waitingForMs > 12000
              ? "down" // never got data after 12s of running — car is off
              : liveState.connected
                ? "waiting"
                : "connecting";
  const UPLINK_META: Record<typeof uplink, { label: string; dot: string }> = {
    idle: { label: "Offline", dot: "dead" },
    connecting: { label: "Connecting…", dot: "stale pulse" },
    // 'waiting' is the brief window right after feed start before any sample
    // arrives — yellow because it MIGHT be transitional. After 12s we escalate
    // to 'down' (red) so a car-off state reads as dead, not as in-between.
    waiting: { label: "No telemetry", dot: "stale" },
    live: { label: "Live", dot: "live" },
    stale: { label: "Delayed", dot: "stale" },
    down: { label: "Signal lost", dot: "dead" },
  };
  const uplinkMeta = UPLINK_META[uplink];
  // "Car telemetry is reachable right now" — a real sample arrived in the last
  // 3.5s. Trackside actions that only make sense with live data (record a lap,
  // log a lap, send a lap to the car) gate on this so they aren't clickable when
  // the car's off or the link is down.
  const telemetryFresh = uplink === "live";

  // Live GPS health — also signals whether GPS auto-lap is backing up the manual
  // lap button (needs an S/F gate defined and a usable fix).
  const sfGateDefined = track.gates.some((gate) => gate.role === "start_finish");
  const liveHasGpsFix = liveState.lastSample != null && hasGps(liveState.lastSample);
  const gpsTag = liveState.lastSample == null
    ? { dot: "stale", label: "GPS —" }
    : liveHasGpsFix
      ? { dot: "live", label: sfGateDefined ? "GPS · auto-lap armed" : "GPS fix" }
      : { dot: "dead", label: "No GPS fix" };

  // Authoritative car state from the central classifier (PR #282), if reachable.
  const carStatus = useCarStatus(source, liveState.running);
  const carStateAgeMs = carStatus.lastEventAt == null ? null : Math.max(0, liveNowMs - carStatus.lastEventAt);
  // Classifier heartbeats every ~2s; treat >7s of silence as untrustworthy.
  const carStateStale = carStateAgeMs != null && carStateAgeMs > 7000;
  const carState: CarState | null = carStatus.available && !carStateStale ? carStatus.state : null;
  const previewSegments = useMemo(
    () => segments.filter((segment) => previewSelectedSegments.has(segment.id)),
    [segments, previewSelectedSegments],
  );
  const previewKey = useMemo(
    () => [...previewSelectedSegments].sort().join("|"),
    [previewSelectedSegments],
  );
  const trackViewKey = useMemo(() => track.gates.map((gate) => `${gate.id}:${gate.lat1}:${gate.lon1}:${gate.lat2}:${gate.lon2}`).join("|"), [track.gates]);
  const selectedTrackView = useMemo(() => trackViewFromGates(track.gates), [trackViewKey]);
  const previewSummary = useMemo(() => summarizeSegments(previewSegments), [previewSegments]);
  const lapPreview = useMemo(() => buildLapPreview(gps, track.gates, previewSummary), [gps, track.gates, previewSummary]);
  const sourceRanges = detail?.sessions ?? (session ? [session] : []);
  const sourceRangeKey = useMemo(
    () => sourceRanges.map((item) => `${item.id}:${item.start_ms}:${item.end_ms}`).join("|"),
    [sourceRanges],
  );
  const exportSegments = useMemo(() => {
    if (range) {
      return [{ id: "manual-selection", label: "Manual plot range", start_ms: range[0], end_ms: range[1], metadata: metadataDraft }];
    }
    return segments
      .filter((s) => selectedSegments.has(s.id))
      .map((s) => ({
        id: s.id,
        label: s.label,
        start_ms: s.start_ms,
        end_ms: s.end_ms,
        metadata: sessionMetadata[s.id] ?? defaultMetadataForSegment(s, sourceLabel, selectedDate),
      }));
  }, [range, segments, selectedSegments, sessionMetadata, metadataDraft, sourceLabel, selectedDate]);

  useEffect(() => {
    refreshBase(source);
  }, [source]);

  useEffect(() => {
    if (!channel) return;
    const timeout = window.setTimeout(() => {
      void refreshCalendar(source, channel, threshold, minDurationS);
    }, 300);
    return () => window.clearTimeout(timeout);
  }, [source, channel, threshold, minDurationS]);

  useEffect(() => {
    document.documentElement.dataset.theme = darkMode ? "dark" : "light";
    localStorage.setItem("motec-theme", darkMode ? "dark" : "light");
  }, [darkMode]);

  useEffect(() => {
    localStorage.setItem("motec-live-topic", liveTopic);
  }, [liveTopic]);

  useEffect(() => {
    localStorage.setItem("motec-live-transport", liveTransport);
  }, [liveTransport]);

  useEffect(() => {
    localStorage.setItem("motec-live-sample-hz", String(liveSampleHz));
  }, [liveSampleHz]);

  useEffect(() => {
    localStorage.setItem("motec-vcu-torque-param-set", torqueParamSet.id);
  }, [torqueParamSet.id]);

  useEffect(() => {
    localStorage.setItem("motec-target-laps", String(targetLaps));
  }, [targetLaps]);

  useEffect(() => {
    localStorage.setItem("motec-target-energy-kwh", String(targetEnergyKwh));
  }, [targetEnergyKwh]);

  useEffect(() => {
    localStorage.setItem("motec-soe-cutoff-cell-v", String(soeCutoffCellV));
  }, [soeCutoffCellV]);

  // Keep the dash uplink connected for the whole session (leader only) so EVERY
  // lap/power/message action reaches the car — not just while the Dash tab is
  // open. Without this, a "Log Lap" pressed from the Live tab would log locally
  // but silently miss the car whenever nobody had hit "Connect to dash" on the
  // Dash tab, making the two lap buttons behave differently. We only auto-connect
  // from the initial 'idle' state, so an explicit Disconnect (→ 'closed') and the
  // hook's own reconnect loop are both respected.
  useEffect(() => {
    if (isMirror) return;
    if (dashSignals.status === "idle") dashSignals.connect();
  }, [isMirror, dashSignals.status, dashSignals.connect]);

  // Persist + live-publish the dash power budget. Re-publishing on every change
  // keeps the on-car dash's energy bar in sync the instant the strategist
  // adjusts it; the hook's keepalive covers the gaps in between.
  useEffect(() => {
    localStorage.setItem("dash-target-power", String(dashTargetPower));
    localStorage.setItem("dash-power-mode", dashPowerMode);
    if (dashSignals.status === "connected" && effectiveTargetPowerKw != null && Number.isFinite(effectiveTargetPowerKw)) {
      dashSignals.publishTargetPower(Math.round(effectiveTargetPowerKw));
    }
  }, [dashTargetPower, dashPowerMode, effectiveTargetPowerKw, dashSignals.status, dashSignals.publishTargetPower]);

  // Push the live dynamic per-lap energy budget (Wh) to the dash whenever it
  // changes, so the driver sees the same Wh/lap target as trackside. Leader only.
  useEffect(() => {
    if (isMirror || dashSignals.status !== "connected") return;
    if (dynamicLapBudgetWh != null) dashSignals.publishLapBudget(Math.round(dynamicLapBudgetWh));
  }, [dynamicLapBudgetWh, isMirror, dashSignals.status, dashSignals.publishLapBudget]);

  // Push the active driver-message set to the car (retained) whenever it changes
  // or the link (re)connects, so the dash holds the current quick-send palette
  // across reboots. Only the leader commands the car.
  useEffect(() => {
    if (isMirror || dashSignals.status !== "connected") return;
    dashSignals.publishMessages(activeMessages(msgLib.lib));
  }, [msgLib.lib, isMirror, dashSignals.status, dashSignals.publishMessages]);

  useEffect(() => {
    localStorage.setItem("dash-auto-lap", dashAutoLap ? "1" : "0");
  }, [dashAutoLap]);

  useEffect(() => {
    localStorage.setItem("flag-sends-message", flagSendsMessage ? "1" : "0");
  }, [flagSendsMessage]);

  useEffect(() => {
    localStorage.setItem("flag-message-map", JSON.stringify(flagMessageMap));
  }, [flagMessageMap]);

  // Persist + publish the lap-card duration to the car (retained, leader only),
  // and re-push on (re)connect so a freshly-booted dash picks it up.
  useEffect(() => {
    localStorage.setItem("dash-lap-card-duration-s", String(lapCardDurationS));
    if (!isMirror && dashSignals.status === "connected") {
      dashSignals.publishLapCardMs(Math.round(lapCardDurationS * 1000));
    }
  }, [lapCardDurationS, isMirror, dashSignals.status, dashSignals.publishLapCardMs]);

  // Keep the active session's registry record current as it runs, so the Log Sync
  // page can match a CSV's loggerd timestamp into the right session window and
  // auto-fill its annotation. Throttled to ~once per 5s so the per-sample stream
  // (~20 Hz) can't hammer localStorage; the window stays accurate to a few sec.
  useEffect(() => {
    if (!sessionInfo) return;
    const now = Date.now();
    if (now - lastRegistryPatchRef.current < 5000) return;
    lastRegistryPatchRef.current = now;
    patchSession(sessionInfo.id, {
      laps: liveStateRef.current.laps.length,
      endedAt: liveStateRef.current.lastSample?.t ?? now,
    });
  }, [sessionInfo, liveState.laps.length, liveState.lastSample?.t]);

  // Keep the active session's FULL record (metadata + energy plan) synced to the
  // server as the strategist edits it (debounced), so the logged session is
  // always complete — not just the name/driver summary.
  useEffect(() => {
    // Leader only: a mirror's plan fields can lag (or sit at the local default)
    // and would clobber the stored session's real plan on the server.
    if (!sessionInfo || isMirror) return;
    const id = window.setTimeout(() => {
      patchSession(sessionInfo.id, {
        metadata: { ...metadataDraft },
        plan: { targetLaps, targetEnergyKwh, soeCutoffCellV },
        setup: { ...driveDaySetup } as Record<string, string | number | boolean>,
      });
    }, 1500);
    return () => window.clearTimeout(id);
  }, [sessionInfo, isMirror, metadataDraft, targetLaps, targetEnergyKwh, soeCutoffCellV, driveDaySetup]);

  // Push drive-day SETUP edits to the drive_day record (debounced) once the
  // session has a linked day. Leader only.
  useEffect(() => {
    if (isMirror || sessionInfo?.dayId == null) return;
    const dayId = sessionInfo.dayId;
    const id = window.setTimeout(() => {
      void fetch("/api/update-drive-day", {
        method: "POST", headers: { "content-type": "application/json" },
        body: JSON.stringify({ day_id: dayId, ...driveDaySetupToPayload(driveDaySetup) }),
      }).catch(() => { /* offline — retried on next edit */ });
    }, 1500);
    return () => window.clearTimeout(id);
  }, [driveDaySetup, sessionInfo, isMirror]);

  // Restore the setup when a session is adopted (reload / mirror / server merge).
  useEffect(() => {
    if (sessionInfo?.setup) setDriveDaySetup(sessionInfo.setup as DriveDaySetup);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [sessionInfo?.id]);

  // On load, reconcile the local session registry with the server (last-writer-
  // wins) and re-push anything the server is missing — so clients converge on
  // the full records and a record that failed to push earlier is retried.
  useEffect(() => { void syncRegistryWithServer(); }, []);

  // Debounced sync of the day's laps (edited times + per-lap notes) to the
  // classifier DB on ANY lap change — add, edit, note, or delete (laps is a new
  // array each time). No-op without a linked drive_day or on a mirror.
  useEffect(() => {
    if (isMirror || sessionInfo?.dayId == null) return;
    if (lapSyncTimerRef.current) window.clearTimeout(lapSyncTimerRef.current);
    const dayId = sessionInfo.dayId;
    const laps = liveState.laps.map((l) => ({ start_time: l.startMs, end_time: l.endMs, notes: l.notes ?? "" }));
    lapSyncTimerRef.current = window.setTimeout(() => {
      void fetch("/api/laps/sync", {
        method: "POST", headers: { "content-type": "application/json" },
        body: JSON.stringify({ day_id: dayId, laps }),
      }).catch(() => { /* offline — retried on next change */ });
    }, 1500);
    return () => { if (lapSyncTimerRef.current) window.clearTimeout(lapSyncTimerRef.current); };
  }, [liveState.laps, sessionInfo, isMirror]);

  // Deep-link from the homepage "Car Status" card (/trackside-live?focus=car):
  // land on the Live tab and scroll the car-status panel into view.
  useEffect(() => {
    if (typeof window === "undefined") return;
    if (new URLSearchParams(window.location.search).get("focus") !== "car") return;
    setActiveTab("live");
    const id = window.setTimeout(() => {
      document.getElementById("car-status-anchor")?.scrollIntoView({ behavior: "smooth", block: "start" });
    }, 200);
    return () => window.clearTimeout(id);
  }, []);

  // Tick once a second while the Dash tab is open so the link-health "Xs ago"
  // ages and the dash-silent warning stay live without a new message.
  useEffect(() => {
    if (activeTab !== "dash") return;
    setDashNow(Date.now());
    const id = window.setInterval(() => setDashNow(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, [activeTab]);

  // Auto-fire a dash lap card whenever the trackside lap detector completes a
  // lap — optional, off by default so the thread's "manual for now" still holds.
  useEffect(() => {
    const count = liveState.laps.length;
    if (count > dashLastLapCountRef.current) {
      if (!isMirror && dashAutoLap && dashSignals.status === "connected") dashSignals.sendLap();
      dashLastLapCountRef.current = count;
    }
  }, [liveState.laps.length, dashAutoLap, dashSignals.status, dashSignals.sendLap, isMirror]);

  // Drain: re-publish the website's lap count whenever the car's mirror shows
  // fewer laps than we've logged. Catches both the dashd-restart and
  // broker-to-dashd drop cases — situations where the original sendLap left
  // the website thinking it succeeded but the car never got the trigger. qos:1
  // on the publish handles the website-side disconnect blip; this handles the
  // far-side gap. dashd's forward-only edge check makes the re-publish a no-op
  // when the car is just temporarily lagged on its state frame.
  useEffect(() => {
    if (isMirror) return;
    if (dashSignals.status !== "connected") return;
    const carLapCount = dashSignals.dashState?.lapCount ?? null;
    if (carLapCount == null) return; // no mirror yet — nothing to compare against
    const websiteLapCount = liveState.laps.length;
    if (websiteLapCount > carLapCount) {
      dashSignals.republishLap(websiteLapCount);
    }
  }, [
    liveState.laps.length,
    dashSignals.status,
    dashSignals.dashState?.lapCount,
    dashSignals.republishLap,
    isMirror,
  ]);

  // Auto-select newly completed laps for the average (preserving manual deselections).
  useEffect(() => {
    setSelectedLapIds((prev) => {
      const next = new Set(prev);
      let changed = false;
      for (const lap of liveState.laps) {
        if (!knownLapIdsRef.current.has(lap.id)) {
          knownLapIdsRef.current.add(lap.id);
          next.add(lap.id);
          changed = true;
        }
      }
      return changed ? next : prev;
    });
  }, [liveState.laps]);

  // Checkpoint the live session while samples are flowing. This is throttled,
  // not debounced, so a constant stream cannot postpone persistence forever.
  useEffect(() => {
    // Persist a session even before its first lap/sample if we have sessionInfo —
    // otherwise a freshly-started session never makes it to the server and a
    // reload drops everything.
    if (!liveState.laps.length && !liveState.samples.length && !sessionInfo) return;
    const now = Date.now();
    const lapCountChanged = liveState.laps.length !== lastSavedLapCountRef.current;
    const shouldSaveLocal = lapCountChanged || now - lastLocalSessionSaveRef.current >= SESSION_LOCAL_AUTOSAVE_MS;
    const shouldSaveBackend = lapCountChanged || now - lastBackendSessionSaveRef.current >= SESSION_BACKEND_AUTOSAVE_MS;
    if (!shouldSaveLocal && !shouldSaveBackend) return;
    persistLiveSession(liveState, {
      saveLocal: shouldSaveLocal,
      saveBackend: shouldSaveBackend,
      includeFullLapSamples: false,
    });
    if (shouldSaveLocal) lastLocalSessionSaveRef.current = now;
    if (shouldSaveBackend) lastBackendSessionSaveRef.current = now;
    lastSavedLapCountRef.current = liveState.laps.length;
  }, [
    liveState.laps,
    liveState.samples,
    liveState.lapStartMs,
    liveState.lapEnergyWh,
    liveState.lapEnergyOutWh,
    liveState.lapEnergyInWh,
    liveState.totalEnergyWh,
    liveState.totalEnergyOutWh,
    liveState.totalEnergyInWh,
    liveState.topic,
    source,
    targetLaps,
    targetEnergyKwh,
    soeCutoffCellV,
    selectedLapIds,
    sessionFromFile,
    sessionInfo,
  ]);

  useEffect(() => {
    const saveBeforeUnload = () => {
      persistLiveSession(liveStateRef.current, {
        saveLocal: true,
        saveBackend: true,
        includeFullLapSamples: false,
      });
    };
    window.addEventListener("pagehide", saveBeforeUnload);
    window.addEventListener("beforeunload", saveBeforeUnload);
    return () => {
      window.removeEventListener("pagehide", saveBeforeUnload);
      window.removeEventListener("beforeunload", saveBeforeUnload);
    };
  }, [source, targetLaps, targetEnergyKwh, soeCutoffCellV, selectedLapIds, sessionFromFile]);

  // On first load, decide whether to restore a previous live session. The
  // session is the SERVER's source of truth — it persists even when every
  // client is gone, so a fresh client picks up the running session without
  // a banner. Only the leader's explicit Stop Event clears it.
  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const candidates: SavedSession[] = [];
      const localSaved = readLocalSavedSession();
      if (isRestorableSession(localSaved)) candidates.push(localSaved);
      const indexedSaved = await readIndexedSavedSession();
      if (isRestorableSession(indexedSaved)) candidates.push(indexedSaved);
      try {
        const backendSaved = await api.latestLiveSession<SavedSession>();
        if (isRestorableSession(backendSaved)) candidates.push(backendSaved);
      } catch {
        // backend cache is optional
      }
      if (cancelled || !candidates.length) return;
      // Take the most recent of local + indexed + backend. The backend cache
      // wins on equality so a freshly-leader client reads the running session.
      const offer = candidates.reduce((best, item) => (item.savedAt >= best.savedAt ? item : best), candidates[0]);
      if (Date.now() - offer.savedAt <= SESSION_AUTO_RESTORE_MAX_AGE_MS) {
        restoreLiveState(offer, false);
        return;
      }
      // Past the auto-restore window but still within the offer window — show
      // the resume banner.
      if (Date.now() - offer.savedAt <= SESSION_RESUME_OFFER_MAX_AGE_MS) {
        setResumeAvailable(offer);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  // Auto-connect to the live stream on first mount so /trackside-live is live
  // without a manual "Start". Uses saved transport/rate. Stop/Reset still work,
  // and we never override an existing/running connection. Guarded so it fires once.
  const autoStartedRef = useRef(false);
  useEffect(() => {
    if (autoStartedRef.current) return;
    autoStartedRef.current = true;
    // Defer one tick so session-restore state settles before we connect.
    const id = window.setTimeout(() => {
      if (!liveStateRef.current.running && !liveSourceRef.current) {
        void startLiveData();
      }
    }, 0);
    return () => window.clearTimeout(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    localStorage.setItem("motec-car-presets", JSON.stringify(carPresets));
  }, [carPresets]);

  useEffect(() => {
    localStorage.setItem("motec-selected-car-preset", selectedCarPresetId);
    const preset = carPresets.find((item) => item.id === selectedCarPresetId);
    if (preset) setCarPresetDraft(preset);
  }, [carPresets, selectedCarPresetId]);

  useEffect(() => {
    if (!selectedTrackView) return;
    setBuilderCenter(selectedTrackView.center);
    setBuilderSearch(`${selectedTrackView.center.lat.toFixed(6)}, ${selectedTrackView.center.lon.toFixed(6)}`);
  }, [selectedTrackView]);

  useEffect(() => {
    liveStateRef.current = liveState;
  }, [liveState]);

  useEffect(() => {
    isMirrorRef.current = isMirror;
  }, [isMirror]);

  // Mirror clients adopt the leader's shared session (laps, totals, lap-in-
  // progress, targets, identity) by polling the session-cache, while keeping
  // their own live gauges from the stream. Skipped entirely for the leader.
  useEffect(() => {
    if (!isMirror) return;
    let cancelled = false;
    const pull = async () => {
      try {
        const saved = await api.latestLiveSession<SavedSession>();
        if (cancelled || !saved) return;
        adoptSharedSession(saved);
      } catch {
        // No shared session yet / unreachable — keep showing live gauges.
      }
    };
    void pull();
    const iv = window.setInterval(pull, 2000);
    return () => { cancelled = true; window.clearInterval(iv); };
  }, [isMirror]);

  useEffect(() => {
    liveTrackRef.current = track;
    liveMetadataRef.current = metadataDraft;
    liveChannelChartRef.current = channelChart;
    liveAutoDownloadRef.current = autoLiveDownload;
    liveSourceNameRef.current = source;
  }, [track, metadataDraft, channelChart, autoLiveDownload, source]);

  useEffect(() => {
    return () => {
      liveShouldRunRef.current = false;
      if (reconnectTimerRef.current != null) window.clearTimeout(reconnectTimerRef.current);
      liveSourceRef.current?.close();
    };
  }, []);

  useEffect(() => {
    if (!selectedDate || !channel) return;
    let cancelled = false;
    setBusy(true);
    setError("");
    api
      .day(source, selectedDate, channel)
      .then((d) => {
        if (cancelled) return;
        setDetail(d);
        setSession(d.sessions[0] ?? null);
        setSegments([]);
        setSelectedSegments(new Set());
        setPreviewSelectedSegments(new Set());
        setLastPreviewSegmentId("");
        setSeries([]);
        setGps([]);
      })
      .catch((e) => {
        if (!cancelled) showError(e);
      })
      .finally(() => {
        if (!cancelled) setBusy(false);
      });
    return () => {
      cancelled = true;
    };
  }, [source, selectedDate, channel]);

  useEffect(() => {
    if (!sourceRanges.length || !channel) return;
    let cancelled = false;
    setRange(null);
    setError("");
    setSegments([]);
    setSelectedSegments(new Set());
    setPreviewSelectedSegments(new Set());
    setLastPreviewSegmentId("");
    setSeries([]);
    setGps([]);

    Promise.all(sourceRanges.map((item) => api.segments(source, channel, item.start_ms, item.end_ms, threshold, minDurationS)))
      .then((responses) => {
        if (cancelled) return;
        const nextSegments = responses
          .flatMap((response) => response.segments)
          .sort((a, b) => a.start_ms - b.start_ms)
          .map((segment, index) => ({ ...segment, label: `Session ${index + 1}` }));
        setSegments(nextSegments);
        setSelectedSegments(new Set(nextSegments.map((segment) => segment.id)));
        setPreviewSelectedSegments(new Set(nextSegments[0] ? [nextSegments[0].id] : []));
        setLastPreviewSegmentId(nextSegments[0]?.id ?? "");
        setSessionMetadata((prev) => {
          const next = { ...prev };
          nextSegments.forEach((segment) => {
            next[segment.id] = next[segment.id] ?? defaultMetadataForSegment(segment, sourceLabel, selectedDate);
          });
          return next;
        });
        setDays((prev) =>
          prev.map((day) =>
            day.date === selectedDate
              ? { ...day, sessions: nextSegments.length, label: `${day.date} (${nextSegments.length} sessions)` }
              : day,
          ),
        );
        setSessionCountsByDate((prev) => ({ ...prev, [selectedDate]: nextSegments.length }));
      })
      .catch((e) => {
        if (!cancelled) showError(e);
      });
    return () => {
      cancelled = true;
    };
  }, [source, sourceRangeKey, channel, threshold, minDurationS, sourceLabel, selectedDate]);

  useEffect(() => {
    if (!channel || !previewSegments.length) {
      setSeries([]);
      setGps([]);
      return;
    }
    let cancelled = false;
    const ordered = [...previewSegments].sort((a, b) => a.start_ms - b.start_ms);
    const seriesPointsPerSegment = Math.max(300, Math.floor(8000 / ordered.length));
    const gpsPointsPerSegment = Math.max(150, Math.floor(4000 / ordered.length));
    setBusy(true);
    Promise.all([
      Promise.all(ordered.map((segment) => api.series(source, channel, segment.start_ms, segment.end_ms, seriesPointsPerSegment))),
      Promise.all(ordered.map((segment) => api.gps(source, segment.start_ms, segment.end_ms, gpsPointsPerSegment))),
    ])
      .then(([seriesResponses, gpsResponses]) => {
        if (cancelled) return;
        setSeries(seriesResponses.flatMap((response) => response.points).sort((a, b) => a.t - b.t));
        setGps(gpsResponses.flatMap((response) => response.points).sort((a, b) => a.t - b.t));
        setError("");
      })
      .catch(showError)
      .finally(() => {
        if (!cancelled) setBusy(false);
      });
    return () => {
      cancelled = true;
    };
  }, [source, channel, previewKey, previewSegments]);

  useEffect(() => {
    if (!previewSegments.length) {
      setMetadataDraft(defaultMetadataBase(sourceLabel, selectedDate));
      setMixedMetadata(new Set());
      return;
    }
    const nextDraft = { ...EMPTY_METADATA };
    const mixed = new Set<MetadataField>();
    METADATA_FIELDS.forEach((field) => {
      const values = previewSegments.map((segment) => (sessionMetadata[segment.id] ?? defaultMetadataForSegment(segment, sourceLabel, selectedDate))[field] ?? "");
      const first = values[0] ?? "";
      if (values.every((value) => value === first)) {
        nextDraft[field] = first;
      } else {
        nextDraft[field] = "";
        mixed.add(field);
      }
    });
    setMetadataDraft(nextDraft);
    setMixedMetadata(mixed);
  }, [previewKey, previewSegments, sessionMetadata, sourceLabel, selectedDate]);

  async function refreshBase(nextSource = source) {
    setBusy(true);
    setError("");
    try {
      const [h, src, c, tr, charts] = await Promise.all([
        api.health(),
        api.sources(),
        api.channels(nextSource),
        api.tracks(),
        api.channelCharts(),
      ]);
      setHealth(h);
      setSources(src.sources.length ? src.sources : DEFAULT_SOURCES);
      setChannels(c.channels);
      setChannel(c.default);
      setSessionCountsByDate({});
      setDays([]);
      setSelectedDate("");
      setTracks(tr.tracks);
      setTrack(tr.tracks[0] ?? DEFAULT_TRACK);
      setChannelCharts(charts.charts);
      setChannelChart(preferredChannelChart(charts.charts, nextSource));
      setDetail(null);
      setSession(null);
    } catch (e) {
      setSources((current) => (current.length ? current : DEFAULT_SOURCES));
      showError(e);
    } finally {
      setBusy(false);
    }
  }

  async function refreshCalendar(nextSource = source, nextChannel = channel, nextThreshold = threshold, nextMinDurationS = minDurationS) {
    if (!nextChannel) return;
    setBusy(true);
    setError("");
    try {
      const cal = await api.calendar(nextSource, nextChannel, nextThreshold, nextMinDurationS, nextThreshold > 0);
      setDays(cal.days);
      setSessionCountsByDate({});
      setSelectedDate((current) => (cal.days.some((day) => day.date === current) ? current : cal.days[0]?.date || ""));
      if (!cal.days.length) {
        setDetail(null);
        setSession(null);
        setSegments([]);
        setSelectedSegments(new Set());
        setPreviewSelectedSegments(new Set());
      }
    } catch (e) {
      showError(e);
    } finally {
      setBusy(false);
    }
  }

  function showError(e: unknown) {
    setError(e instanceof Error ? e.message : String(e));
  }

  function toggleSegment(segment: SegmentSummary) {
    setRange(null);
    setSelectedSegments((prev) => {
      const next = new Set(prev);
      if (next.has(segment.id)) next.delete(segment.id);
      else next.add(segment.id);
      return next;
    });
  }

  function handleSessionClick(event: MouseEvent, segment: SegmentSummary) {
    setRange(null);
    const segmentIds = segments.map((item) => item.id);
    const currentIndex = segmentIds.indexOf(segment.id);
    const lastIndex = segmentIds.indexOf(lastPreviewSegmentId);
    if (event.shiftKey && lastIndex >= 0 && currentIndex >= 0) {
      const [start, end] = [lastIndex, currentIndex].sort((a, b) => a - b);
      setPreviewSelectedSegments(new Set(segmentIds.slice(start, end + 1)));
    } else if (event.metaKey || event.ctrlKey) {
      setPreviewSelectedSegments((prev) => {
        const next = new Set(prev);
        if (next.has(segment.id)) next.delete(segment.id);
        else next.add(segment.id);
        return next.size ? next : new Set([segment.id]);
      });
    } else {
      setPreviewSelectedSegments(new Set([segment.id]));
    }
    setLastPreviewSegmentId(segment.id);
  }

  function selectAllSessionsForPreview() {
    setRange(null);
    setPreviewSelectedSegments(new Set(segments.map((segment) => segment.id)));
    setLastPreviewSegmentId(segments.at(-1)?.id ?? "");
  }

  function applyMetadata() {
    const selected = previewSegments;
    if (!selected.length) return;
    setSessionMetadata((prev) => {
      const next = { ...prev };
      selected.forEach((segment) => {
        const current = next[segment.id] ?? defaultMetadataForSegment(segment, sourceLabel, selectedDate);
        const updated = { ...current };
        METADATA_FIELDS.forEach((field) => {
          if (!mixedMetadata.has(field) || metadataDraft[field] !== "") {
            updated[field] = metadataDraft[field];
          }
        });
        next[segment.id] = updated;
      });
      return next;
    });
  }

  function handleDrawGate(gate: GateLine) {
    setTrack((prev) => {
      if (gate.role === "start_finish" && prev.gates.some((item) => item.role === "start_finish")) return prev;
      return normalizeTrack({ ...prev, gates: [...prev.gates, gate] });
    });
    setGateDrawMode(null);
  }

  function updateGate(index: number, patch: Partial<GateLine>) {
    setTrack((prev) => ({
      ...prev,
      gates: prev.gates.map((gate, gateIndex) => (gateIndex === index ? { ...gate, ...patch } : gate)),
    }));
  }

  function removeGate(index: number) {
    setTrack((prev) => normalizeTrack({ ...prev, gates: prev.gates.filter((_, gateIndex) => gateIndex !== index) }));
  }

  function moveSplitGate(index: number, direction: -1 | 1) {
    setTrack((prev) => {
      const gate = prev.gates[index];
      if (!gate || gate.role !== "split") return prev;
      const splitIndexes = prev.gates.flatMap((item, itemIndex) => (item.role === "split" ? [itemIndex] : []));
      const splitPosition = splitIndexes.indexOf(index);
      const targetIndex = splitIndexes[splitPosition + direction];
      if (targetIndex == null) return prev;
      const gates = [...prev.gates];
      [gates[index], gates[targetIndex]] = [gates[targetIndex], gates[index]];
      return normalizeTrack({ ...prev, gates });
    });
  }

  async function saveTrack(nextTrack = track) {
    setBusy(true);
    try {
      const saved = await api.saveTrack(normalizeTrack(nextTrack));
      setTrack(saved);
      const list = await api.tracks();
      setTracks(list.tracks);
    } catch (e) {
      showError(e);
    } finally {
      setBusy(false);
    }
  }

  function newTrack() {
    setTrack({ name: "New Track", slug: "", notes: "", gates: [] });
    setGateDrawMode(null);
  }

  function downloadTrack() {
    const blob = new Blob([JSON.stringify(track, null, 2) + "\n"], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `${slugifyTrackName(track.name)}.json`;
    link.click();
    URL.revokeObjectURL(url);
  }

  async function uploadTrack(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    try {
      const uploaded = JSON.parse(await file.text()) as TrackDefinition;
      if (!uploaded.name || !Array.isArray(uploaded.gates)) throw new Error("Track JSON is missing a name or gates array.");
      await saveTrack({
        ...DEFAULT_TRACK,
        ...uploaded,
        slug: uploaded.slug || slugifyTrackName(uploaded.name),
        gates: uploaded.gates,
      });
    } catch (e) {
      showError(e);
    }
  }

  async function saveChannelChart(nextChart = channelChart) {
    setBusy(true);
    try {
      const saved = await api.saveChannelChart(nextChart);
      setChannelChart(saved);
      const list = await api.channelCharts();
      setChannelCharts(list.charts);
    } catch (e) {
      showError(e);
    } finally {
      setBusy(false);
    }
  }

  function downloadChannelChart() {
    const blob = new Blob([JSON.stringify(channelChart, null, 2) + "\n"], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `${slugifyTrackName(channelChart.name)}.json`;
    link.click();
    URL.revokeObjectURL(url);
  }

  async function uploadChannelChart(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    try {
      const text = await file.text();
      const uploaded = file.name.toLowerCase().endsWith(".json")
        ? parseChannelChartJson(text, file.name)
        : parseChannelChartCsv(text, file.name);
      await saveChannelChart(uploaded);
    } catch (e) {
      showError(e);
    }
  }

  function applyCarPreset(preset = carPresetDraft) {
    setSelectedCarPresetId(preset.id);
    setSource(preset.source);
    setLiveTopic(preset.topic);
    setLiveTransport(preset.transport);
    setMetadataDraft({ ...EMPTY_METADATA, ...preset.metadata });
    const presetTrack = tracks.find((item) => item.slug === preset.trackSlug);
    if (presetTrack) setTrack(normalizeTrack(presetTrack));
    const presetChart = channelCharts.find((item) => item.slug === preset.channelChartSlug);
    if (presetChart) setChannelChart(presetChart);
  }

  function saveCarPreset() {
    const normalized = normalizeCarPreset({
      ...carPresetDraft,
      id: carPresetDraft.id || slugifyTrackName(carPresetDraft.name),
      name: carPresetDraft.name || sourceLabel,
      topic: carPresetDraft.topic.trim() || `grafana_data_${carPresetDraft.source}`,
      trackSlug: carPresetDraft.trackSlug || track.slug,
      channelChartSlug: carPresetDraft.channelChartSlug || channelChart.slug,
    });
    if (!normalized) return;
    setCarPresets((current) => {
      const existing = current.some((item) => item.id === normalized.id);
      return existing ? current.map((item) => (item.id === normalized.id ? normalized : item)) : [...current, normalized];
    });
    setSelectedCarPresetId(normalized.id);
    setCarPresetDraft(normalized);
  }

  function newCarPreset() {
    const id = `car-${Date.now()}`;
    const preset: CarPreset = {
      id,
      name: "New Car",
      source,
      topic: `grafana_data_${source}`,
      transport: "local",
      trackSlug: track.slug,
      channelChartSlug: channelChart.slug,
      metadata: { ...EMPTY_METADATA, vehicle_id: "New Car", vehicle_type: "EV" },
    };
    setCarPresets((current) => [...current, preset]);
    setSelectedCarPresetId(id);
    setCarPresetDraft(preset);
  }

  function deleteCarPreset() {
    const next = carPresets.filter((item) => item.id !== carPresetDraft.id);
    const fallback = next[0] ?? defaultCarPresets()[0];
    setCarPresets(next.length ? next : [fallback]);
    setSelectedCarPresetId(fallback.id);
    setCarPresetDraft(fallback);
  }

  async function startLiveData(sourceOverride?: "orion" | "angelique") {
    const useSource = sourceOverride ?? source;
    setEventEnded(false);
    liveSourceRef.current?.close();
    liveShouldRunRef.current = true;
    if (reconnectTimerRef.current != null) {
      window.clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }
    setExportUrl("");
    const existing = liveStateRef.current;
    const shouldResumeSession = !!(existing.laps.length || existing.samples.length || existing.lapStartMs);
    const nextLiveState: LiveSessionState = {
      ...(shouldResumeSession ? existing : EMPTY_LIVE_STATE),
      running: true,
      connected: false,
      status: liveTransport === "local" ? "Connecting to local replay topic..." : liveTransport === "mqtt" ? "Connecting to MQTT..." : "Connecting to Kafka...",
      startedAt: existing.startedAt ?? Date.now(),
    };
    liveStateRef.current = nextLiveState;
    setLiveState(nextLiveState);
    try {
      const requestedTopic = liveTopic.trim();
      const config = await api.liveConfig(useSource, requestedTopic, liveTransport);
      setLiveState((prev) => ({ ...prev, topic: config.topic, status: liveTransport === "mqtt" ? `Listening to MQTT ${config.topic}` : `Listening to ${config.topic}` }));
      const params = new URLSearchParams({ source: useSource, topic: requestedTopic, transport: liveTransport, sampleHz: String(liveSampleHz) });
      const eventSource = new EventSource(`/api/motec/live/stream?${params.toString()}`);
      liveSourceRef.current = eventSource;
      eventSource.onopen = () => {
        setLiveState((prev) => ({ ...prev, connected: true }));
      };
      eventSource.onmessage = (event) => {
        try {
          const parsed = JSON.parse(event.data) as LiveStreamEvent;
          handleLiveEvent(parsed);
        } catch (e) {
          setLiveState((prev) => ({ ...prev, status: e instanceof Error ? e.message : String(e) }));
        }
      };
      eventSource.onerror = () => {
        liveSourceRef.current?.close();
        liveSourceRef.current = null;
        setLiveState((prev) => (prev.running ? { ...prev, connected: false, status: "Reconnecting to live stream…" } : prev));
        scheduleLiveReconnect();
      };
    } catch (e) {
      showError(e);
      setLiveState((prev) => ({ ...prev, connected: false, status: "Live data unreachable — retrying…" }));
      scheduleLiveReconnect();
    }
  }

  // Auto-reconnect: the manual Start button was removed, so a dropped or
  // unreachable stream retries on its own until stop/reset clears the intent.
  function scheduleLiveReconnect() {
    if (!liveShouldRunRef.current || reconnectTimerRef.current != null) return;
    reconnectTimerRef.current = window.setTimeout(() => {
      reconnectTimerRef.current = null;
      if (liveShouldRunRef.current && !liveSourceRef.current) void startLiveData();
    }, 4000);
  }

  function stopLiveData() {
    liveShouldRunRef.current = false;
    if (reconnectTimerRef.current != null) {
      window.clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }
    liveSourceRef.current?.close();
    liveSourceRef.current = null;
    setLiveState((prev) => ({ ...prev, running: false, connected: false, status: prev.laps.length ? "Stopped" : "Stopped without completed laps" }));
  }

  // Wipe all live-session state + caches. Does NOT touch the feed's run-intent —
  // callers decide whether to re-arm the stream afterwards.
  function clearLiveSessionState() {
    if (reconnectTimerRef.current != null) {
      window.clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }
    liveSourceRef.current?.close();
    liveSourceRef.current = null;
    liveStateRef.current = EMPTY_LIVE_STATE;
    knownLapIdsRef.current = new Set();
    lastLocalSessionSaveRef.current = 0;
    lastBackendSessionSaveRef.current = 0;
    lastSavedLapCountRef.current = 0;
    dashLastLapCountRef.current = 0;
    lastRegistryPatchRef.current = 0;
    setLiveState(EMPTY_LIVE_STATE);
    setSelectedLapIds(new Set());
    setResumeAvailable(null);
    removeLocalSavedSession();
    void removeIndexedSavedSession();
    void api.clearLiveSession().catch(() => undefined);
  }

  function resetLiveSession() {
    if (isMirrorRef.current) return;
    const hasSessionData = liveStateRef.current.laps.length || liveStateRef.current.samples.length || liveStateRef.current.lapStartMs;
    if (hasSessionData && !window.confirm("Reset all live laps, current lap data, and saved session cache?")) return;
    if (sessionInfo) {
      patchSession(sessionInfo.id, { endedAt: Date.now(), laps: liveStateRef.current.laps.length });
      setSessionInfo(null);
    }
    clearLiveSessionState();
    // The manual Start button is gone, so a reset re-arms the auto feed; the
    // cleared session keeps streaming rather than stranding the strategist.
    void startLiveData();
  }

  // Start a fresh, named session: close out any prior record, clear state, stamp
  // the metadata for exports + the logsync registry, and re-arm the feed.
  function startNewSession(draft: { name: string; car: "orion" | "angelique"; driver: string; eventType: string; venue: string }) {
    if (isMirrorRef.current) return;
    if (sessionInfo) patchSession(sessionInfo.id, { endedAt: Date.now(), laps: liveStateRef.current.laps.length });
    clearLiveSessionState();
    const startedAt = Date.now();
    const info: TracksideSessionInfo = {
      id: `sess-${startedAt}-${Math.floor(Math.random() * 1e6)}`,
      name: draft.name.trim() || `${draft.venue || "Session"} ${new Date(startedAt).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}`,
      car: draft.car,
      driver: draft.driver.trim(),
      eventType: draft.eventType,
      venue: draft.venue.trim(),
      startedAt,
      endedAt: null,
      laps: 0,
      // Full record from the start — the metadata draft + energy plan ride
      // along so the server logs the complete session, not just the summary.
      metadata: { ...metadataDraft, driver: draft.driver.trim(), venue: draft.venue.trim(), event: draft.eventType, session: draft.name.trim() },
      plan: { targetLaps, targetEnergyKwh, soeCutoffCellV },
      setup: { ...driveDaySetup } as Record<string, string | number | boolean>,
      // Stamp the starter so this user can reclaim leader on rejoin.
      starterId: presence.clientId || undefined,
    };
    upsertSession(info);
    setSessionInfo(info);
    // Back the session with a drive_day record (status=2) so event flags + laps
    // persist to the classifier. Sequenced (end prior → create) so end-event
    // can't close the day we just made; async + resilient (session works regardless).
    void (async () => {
      await endActiveDriveDay();
      const dayId = await createDriveDayForSession(info);
      if (dayId != null) {
        setSessionInfo((p) => (p && p.id === info.id ? { ...p, dayId } : p));
        patchSession(info.id, { dayId });
      }
    })();
    // Mirror into the MoTeC/CSV export metadata so a downloaded lap carries the
    // same identity the logsync annotation will get.
    setMetadataDraft((prev) => ({ ...prev, driver: info.driver, venue: info.venue, event: info.eventType, session: info.name }));
    if (draft.car !== source) setSource(draft.car);
    setNewSessionOpen(false);
    // Tell dashd to drop its stale lap counter + baseline so the new session
    // starts at lap 1 on both sides. Without this, dashd would still hold the
    // prior session's high water mark and silently ignore the first few
    // sendLap publishes (1 < N).
    if (dashSignals.status === "connected") dashSignals.resetLapCounter();
    void startLiveData(draft.car);
  }

  // Publish the lap-card layout to the car (retained: used until replaced). Only
  // the session leader commands the car; the dash link must be connected.
  function sendLapLayout(layout: LapCardLayout) {
    if (isMirrorRef.current) { setLayoutSendStatus("Read-only mirror — only the session leader can push to the car."); return; }
    if (dashSignals.status !== "connected") { setLayoutSendStatus("Connect to the dash first (Dash tab → Connect to dash)."); return; }
    const ok = dashSignals.publishLayout(layout);
    setLayoutSendStatus(ok ? `Sent “${layout.name}” to the car ✓ (retained — used until replaced)` : "Publish failed — check the dash link.");
    window.setTimeout(() => setLayoutSendStatus(""), 5000);
  }

  // Fire one driver message onto the dash now. Same leader + link gating as the
  // other car commands.
  function sendDriverMessage(m: DashMessage) {
    if (isMirrorRef.current) { setMsgSendStatus("Read-only mirror — only the session leader can message the driver."); return; }
    if (dashSignals.status !== "connected") { setMsgSendStatus("Connect to the dash first (Dash tab → Connect to dash)."); return; }
    const ok = dashSignals.sendMessage(m);
    setMsgSendStatus(ok ? `Sent “${m.label}” to the driver ✓${m.durationS ? ` (${m.durationS}s)` : " (until cleared)"}` : "Send failed — check the dash link.");
    window.setTimeout(() => setMsgSendStatus(""), 4000);
  }

  function clearDriverMessage() {
    if (isMirrorRef.current || dashSignals.status !== "connected") return;
    if (dashSignals.clearMessage()) {
      setMsgSendStatus("Cleared the driver's message.");
      window.setTimeout(() => setMsgSendStatus(""), 4000);
    }
  }

  // Download the session's laps as a flat CSV (timestamps + per-lap energy), for
  // sharing/analysis alongside the MoTeC export and the full session JSON.
  function downloadSessionCsv() {
    const laps = liveStateRef.current.laps;
    if (!laps.length) return;
    const header = ["lap", "kind", "start_iso", "end_iso", "duration_s", "energy_out_wh", "energy_in_wh", "distance_m", "avg_speed_mps"];
    const rows = laps.map((lap, i) => [
      i + 1,
      lap.kind,
      new Date(lap.startMs).toISOString(),
      new Date(lap.endMs).toISOString(),
      (lap.durationMs / 1000).toFixed(3),
      (lap.energyOutWh ?? lap.energyWh).toFixed(1),
      (lap.energyInWh ?? 0).toFixed(1),
      lap.distanceM.toFixed(1),
      lap.avgSpeedMps != null ? lap.avgSpeedMps.toFixed(2) : "",
    ]);
    const csv = [header, ...rows].map((r) => r.join(",")).join("\n");
    const blob = new Blob([csv], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    const base = (sessionInfo?.name || `${source}_session`).replace(/[^\w.-]+/g, "_");
    link.href = url;
    link.download = `${base}_laps.csv`;
    link.click();
    URL.revokeObjectURL(url);
  }

  // Full session as JSON — laps, sectors, per-lap energy, the captured sample
  // tail, metadata and session identity: everything to review the session
  // offline EXCEPT the raw car CSVs (those are fetched via Log Sync).
  function downloadSessionJson() {
    const saved = buildSavedSession(liveStateRef.current, true);
    // Surface the live drive-day setup + plan at the top level so a reviewer
    // gets the conditions/car-setup/tires/aero in one obvious place without
    // walking sessionInfo. (sessionInfo.setup is still the canonical copy.)
    const full = {
      ...saved,
      metadata: { ...liveMetadataRef.current },
      plan: { targetLaps, targetEnergyKwh, soeCutoffCellV },
      driveDaySetup: { ...driveDaySetupRef.current },
      dayId: sessionInfo?.dayId ?? null,
      exportedAt: Date.now(),
    };
    const blob = new Blob([JSON.stringify(full, null, 2) + "\n"], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    const base = (sessionInfo?.name || `${source}_session`).replace(/[^\w.-]+/g, "_");
    link.href = url;
    link.download = `${base}_session.json`;
    link.click();
    URL.revokeObjectURL(url);
  }

  // Stop Event: end the session and download everything needed to review it
  // locally (laps CSV + full session JSON), then stop the live feed. Leaves the
  // session on screen. Does NOT pull the raw car CSVs — Log Sync handles those.
  function stopEventAndDownload() {
    if (isMirrorRef.current) return;
    if (!liveStateRef.current.laps.length && !liveStateRef.current.samples.length) {
      setLiveState((prev) => ({ ...prev, status: "Nothing to stop — no session data yet." }));
      return;
    }
    // Capture the data first — blob downloads are synchronous, so the files
    // grab a snapshot of liveStateRef.current before we clear it below.
    const lapsExported = liveStateRef.current.laps.length;
    downloadSessionCsv();
    downloadSessionJson();
    // Flush the debounced laps-sync now so the FINAL lap set lands in the DB
    // even if Stop is clicked within 1500 ms of the last lap. Cancel the
    // queued timer first so it can't race with the clear below.
    if (sessionInfo?.dayId != null) {
      if (lapSyncTimerRef.current) { window.clearTimeout(lapSyncTimerRef.current); lapSyncTimerRef.current = null; }
      const dayId = sessionInfo.dayId;
      const laps = liveStateRef.current.laps.map((l) => ({ start_time: l.startMs, end_time: l.endMs, notes: l.notes ?? "" }));
      void fetch("/api/laps/sync", {
        method: "POST", headers: { "content-type": "application/json" }, keepalive: true,
        body: JSON.stringify({ day_id: dayId, laps }),
      }).catch(() => { /* offline — JSON download still has them */ });
    }
    if (sessionInfo) {
      patchSession(sessionInfo.id, { endedAt: Date.now(), laps: lapsExported });
      setSessionInfo(null);
      void endActiveDriveDay(); // close the drive_day record (status → 0)
    }
    // Stopping the event ENDS the session — clear the laps/samples/energy too
    // so the hero reads clean for the next session. The downloaded files are
    // the on-screen review. liveShouldRunRef is cleared so the feed doesn't
    // auto-reconnect; a new session will re-arm it.
    liveShouldRunRef.current = false;
    // Reset the car's lap_count too so the dash starts the next session at
    // Lap 0 instead of holding this session's high water mark. Same publish
    // as startNewSession — symmetrical with how a new session starts.
    if (dashSignals.status === "connected") dashSignals.resetLapCounter();
    clearLiveSessionState();
    setEventEnded(true);
    setLiveState((prev) => ({ ...prev, status: `Event stopped — ${lapsExported} lap${lapsExported === 1 ? "" : "s"} downloaded, session cleared.` }));
  }

  // Manually correct a lap's time (timing glitch / late press). Keeps the lap's
  // start, sets its duration + end. Persists so it survives reload + syncs.
  function editLapDuration(id: string, durationMs: number) {
    if (isMirrorRef.current || !Number.isFinite(durationMs) || durationMs < 0) return;
    const cur = liveStateRef.current;
    const next = { ...cur, laps: cur.laps.map((l) => (l.id === id ? { ...l, durationMs, endMs: l.startMs + durationMs } : l)) };
    liveStateRef.current = next;
    setLiveState(next);
    persistLiveSession(next, { saveLocal: true, saveBackend: true, includeFullLapSamples: false });
  }

  // Delete a lap (accident / bad data). Drops it from the average selection too.
  function deleteLap(id: string) {
    if (isMirrorRef.current) return;
    const lap = liveStateRef.current.laps.find((l) => l.id === id);
    if (lap && !window.confirm(`Delete ${lap.label} (${formatLapTime(lap.durationMs)})? This can't be undone.`)) return;
    const cur = liveStateRef.current;
    const next = { ...cur, laps: cur.laps.filter((l) => l.id !== id) };
    liveStateRef.current = next;
    setLiveState(next);
    setSelectedLapIds((prev) => { const n = new Set(prev); n.delete(id); return n; });
    persistLiveSession(next, { saveLocal: true, saveBackend: true, includeFullLapSamples: false });
  }

  // --- Session ⇒ drive_day link + event flags (autocross incidents) ----------
  async function endActiveDriveDay() {
    try { await fetch("/api/end-event", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ car: source }) }); } catch { /* offline — non-fatal */ }
  }
  // Create the drive_day record that backs this trackside session (status=2),
  // so event flags + laps persist to the classifier. Returns the day_id or null.
  async function createDriveDayForSession(info: TracksideSessionInfo): Promise<number | null> {
    try {
      const res = await fetch("/api/new-drive-day", {
        method: "POST", headers: { "content-type": "application/json" },
        body: JSON.stringify({
          date: new Date(info.startedAt).toISOString().slice(0, 10),
          ...driveDaySetupToPayload(driveDaySetupRef.current),
          track_name: info.venue || track.name || null,
          car_id: carIdForSource(info.car),
          driver_id: 0,
          event_type: eventTypeIdFor(info.eventType),
          location_id: 0,
        }),
      });
      if (!res.ok) return null;
      const data = await res.json().catch(() => ({}));
      return typeof data?.day_id === "number" ? data.day_id : null;
    } catch { return null; }
  }
  // Fire a trackside event flag (cone / off-track / incomplete / custom) — lands
  // in the classifier table against the active drive_day.
  async function eventFlag(label: string) {
    if (isMirrorRef.current || !label.trim()) return;
    if (!sessionInfo) { setLiveState((prev) => ({ ...prev, status: "Start a session before flagging events." })); return; }
    // Optionally flash the same flag on the driver's dash.
    if (flagSendsMessage) fireFlagDriverMessage(label);
    try {
      const res = await fetch("/api/event-flag", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ eventFlag: label }) });
      setLiveState((prev) => ({ ...prev, status: res.ok ? `Flagged: ${label}` : "Flag failed — no active drive day on the server." }));
    } catch { setLiveState((prev) => ({ ...prev, status: "Flag failed — offline?" })); }
  }
  // Flash an event flag on the driver's dash (used when "Show on driver dash" is
  // on). The per-flag picker decides which screen: a chosen message-library
  // screen, "none" (skip), or "auto" (build one from the flag text below).
  function fireFlagDriverMessage(label: string) {
    if (isMirrorRef.current) return;
    if (dashSignals.status !== "connected") {
      setMsgSendStatus(`Flag "${label}" not sent to driver — dash uplink ${dashSignals.status}.`);
      window.setTimeout(() => setMsgSendStatus(""), 4000);
      return;
    }
    const choice = flagMessageMap[label];
    if (choice === "none") {
      setMsgSendStatus(`Flag "${label}" logged (no driver message — "Don't send" selected).`);
      window.setTimeout(() => setMsgSendStatus(""), 4000);
      return;
    }
    if (choice && choice !== "auto") {
      const picked = msgLib.lib.items.find((m) => m.id === choice);
      if (picked) {
        const ok = dashSignals.sendMessage(picked, picked.durationS || undefined);
        setMsgSendStatus(ok ? `Sent "${picked.label}" to driver (flag: ${label}) ✓` : `Send failed — check the dash link.`);
        window.setTimeout(() => setMsgSendStatus(""), 4000);
        return;
      }
      // picked screen was deleted — fall through to the auto-built one.
      setMsgSendStatus(`Picked screen for "${label}" was deleted; sending auto fallback.`);
    }
    const key = label.toLowerCase();
    const style: { icon: DashMessage["icon"]; color: string } =
      key.includes("cone") ? { icon: "warning", color: "#f5a524" }
      : key.includes("off") ? { icon: "warning", color: "#ff4d4f" }
      : key.includes("incomplete") || key.includes("dnf") ? { icon: "flag", color: "#ff4d4f" }
      : { icon: "warning", color: "#f5a524" };
    const msg: DashMessage = {
      id: `flag-${label}`,
      label,
      text: label.toUpperCase(),
      icon: style.icon,
      color: style.color,
      durationS: 4,
    };
    const ok = dashSignals.sendMessage(msg, 4);
    setMsgSendStatus(ok ? `Sent auto "${label}" to driver ✓` : `Send failed — check the dash link.`);
    window.setTimeout(() => setMsgSendStatus(""), 4000);
  }
  function customFlag() {
    const note = window.prompt("Custom event flag (note):");
    if (note && note.trim()) void eventFlag(note.trim());
  }

  // Set/clear a per-lap note. Persists with the session; the sync effect pushes
  // it to the classifier DB.
  function editLapNote(id: string, notes: string) {
    if (isMirrorRef.current) return;
    const cur = liveStateRef.current;
    const next = { ...cur, laps: cur.laps.map((l) => (l.id === id ? { ...l, notes } : l)) };
    liveStateRef.current = next;
    setLiveState(next);
    persistLiveSession(next, { saveLocal: true, saveBackend: true, includeFullLapSamples: false });
  }

  function handleLiveEvent(event: LiveStreamEvent) {
    if (event.type === "status") {
      if (!event.ok) {
        liveSourceRef.current?.close();
        liveSourceRef.current = null;
      }
      setLiveState((prev) => ({
        ...prev,
        running: event.ok ? prev.running : false,
        connected: event.ok,
        status: event.ok ? event.message : `${event.message}${event.detail ? ` ${event.detail}` : ""}`,
        topic: event.topic || prev.topic,
      }));
      return;
    }
    if (event.type === "heartbeat") {
      setLiveState((prev) => ({
        ...prev,
        connected: true,
        topic: event.topic || prev.topic,
        status: prev.lastSample
          ? prev.status
          : event.transport === "mqtt"
            ? `MQTT connected; waiting for samples on ${event.topic || prev.topic}.`
            : `Broker connected; waiting for samples on ${event.topic || prev.topic}.`,
      }));
      return;
    }
    if (event.type !== "sample") return;
    setLastDataAtMs(Date.now());
    const completedLap = processLiveSample(event.sample);
    if (completedLap && liveAutoDownloadRef.current) {
      void downloadLiveLap(completedLap);
    }
  }

  function processLiveSample(sample: LiveSample) {
    const current = liveStateRef.current;
    // Mirror clients don't run lap detection (the leader owns lap boundaries and
    // we adopt its doc) — but they DO integrate session energy from their own
    // feed. Every client gets the same car telemetry, so this stays in lockstep
    // with the leader while making the hero Energy/Regen totals immune to a
    // leader whose tab is on a stale build (which would otherwise broadcast a
    // zeroed total that the mirror blindly adopted). adoptSharedSession keeps
    // these locally-integrated totals rather than clobbering them.
    if (isMirrorRef.current) {
      const prev = current.lastSample;
      // VCU is the source of truth for energy. No client-side integrator;
      // we just relay the per-sample DELTAS of the VCU's cumulative
      // net_energy + regen_energy. Energy reads 0 until the VCU emits 0x1C9.
      const { outDeltaWh, inDeltaWh } = vcuEnergyDeltaWh(prev, sample);
      const mirrored: LiveSessionState = {
        ...current,
        previousSample: current.lastSample,
        lastSample: sample,
        samples: [...current.samples, sample].slice(-LIVE_SAMPLE_MEMORY_CAP),
        totalEnergyOutWh: current.totalEnergyOutWh + outDeltaWh,
        totalEnergyInWh: current.totalEnergyInWh + inDeltaWh,
        totalEnergyWh: current.totalEnergyWh + (outDeltaWh - inDeltaWh),
      };
      liveStateRef.current = mirrored;
      setLiveState(mirrored);
      return;
    }
    const previous = current.lastSample;
    const trackForLive = liveTrackRef.current;
    let completedLap: LiveLap | null = null;
    const dtSeconds = previous ? Math.max(0, (sample.t - previous.t) / 1000) : 0;
    // VCU is the source of truth for energy — read it from 0x1C9 (net + regen
    // cumulative Wh) per sample, relay the deltas. We DON'T integrate
    // power×dt on the client any more. Until the VCU emits 0x1C9, the deltas
    // are 0 and the website shows 0 Wh — that's correct: we don't know the
    // truth so we don't fake it.
    const { outDeltaWh: energyOutDeltaWh, inDeltaWh: energyInDeltaWh } = vcuEnergyDeltaWh(previous, sample);
    const energyDeltaWh = energyOutDeltaWh - energyInDeltaWh;
    const distanceDeltaM = previous && hasGps(previous) && hasGps(sample) ? distanceMeters(previous.lat, previous.lon, sample.lat, sample.lon) : 0;
    let lapStartMs = current.lapStartMs;
    let sectorStartMs = current.sectorStartMs;
    let nextSplitIndex = current.nextSplitIndex;
    let currentSectors = [...current.currentSectors];
    let lapEnergyWh = current.lapEnergyWh + energyDeltaWh;
    let lapEnergyOutWh = current.lapEnergyOutWh + energyOutDeltaWh;
    let lapEnergyInWh = current.lapEnergyInWh + energyInDeltaWh;
    let lapDistanceM = current.lapDistanceM + distanceDeltaM;
    let lapSamples = [...current.lapSamples, sample].slice(-LIVE_LAP_SAMPLE_MEMORY_CAP);
    let resetDelta = false;
    const laps = [...current.laps];

    // Trust a GPS step for lap/sector detection only if both points have a fix
    // and the implied speed is plausible — a teleport spike is a bad fix, not a
    // real crossing. (Sector splits now also run live, not just on file replay,
    // so GPS auto-timing backs up the manual lap button for redundancy.)
    const gpsStepSpeed = dtSeconds > 0 ? distanceDeltaM / dtSeconds : 0;
    const gpsTrustworthy = previous != null && hasGps(previous) && hasGps(sample) && gpsStepSpeed <= MAX_PLAUSIBLE_GPS_MPS;
    if (gpsTrustworthy) {
      const startGate = trackForLive.gates.find((gate) => gate.role === "start_finish");
      const splits = trackForLive.gates.filter((gate) => gate.role === "split");
      if (startGate && sampleCrossesGate(previous, sample, startGate)) {
        if (lapStartMs && sample.t - lapStartMs > 5000) {
          const durationMs = sample.t - lapStartMs;
          const finalSectorMs = sectorStartMs ? sample.t - sectorStartMs : durationMs;
          const sectors = splits.length && finalSectorMs > 0 ? [...currentSectors, finalSectorMs] : [];
          completedLap = {
            id: `live-lap-${laps.length + 1}-${sample.t}`,
            label: `Lap ${laps.length + 1}`,
            kind: "flying",
            startMs: lapStartMs,
            endMs: sample.t,
            durationMs,
            sectors,
            energyWh: lapEnergyWh,
            energyOutWh: lapEnergyOutWh,
            energyInWh: lapEnergyInWh,
            distanceM: lapDistanceM,
            avgSpeedMps: durationMs > 0 && lapDistanceM > 0 ? lapDistanceM / (durationMs / 1000) : null,
            samples: [],
          };
          laps.push(completedLap);
        }
        lapStartMs = sample.t;
        sectorStartMs = sample.t;
        nextSplitIndex = 0;
        currentSectors = [];
        lapEnergyWh = 0;
        lapEnergyOutWh = 0;
        lapEnergyInWh = 0;
        lapDistanceM = 0;
        lapSamples = [sample];
        resetDelta = true;
      } else if (lapStartMs && nextSplitIndex < splits.length) {
        const nextGate = splits[nextSplitIndex];
        if (sampleCrossesGate(previous, sample, nextGate)) {
          const sectorStart = sectorStartMs ?? lapStartMs;
          const sectorMs = sample.t - sectorStart;
          if (sectorMs > 500) {
            currentSectors.push(sectorMs);
            sectorStartMs = sample.t;
            nextSplitIndex += 1;
          }
        }
      }
    }

    const best = laps.reduce<LiveLap | null>((candidate, lap) => (!candidate || lap.durationMs < candidate.durationMs ? lap : candidate), null);
    const deltaRate = estimateDeltaRate(sample, best);
    const deltaMs = resetDelta ? 0 : lapStartMs && deltaRate != null ? current.deltaMs + deltaRate * dtSeconds * 1000 : current.deltaMs;
    const nextState = {
      ...current,
      running: true,
      connected: true,
      status: current.topic ? `Live on ${current.topic}` : "Live",
      lastSample: sample,
      previousSample: previous,
      samples: [...current.samples, sample].slice(-LIVE_SAMPLE_MEMORY_CAP),
      lapSamples,
      laps,
      lapStartMs,
      sectorStartMs,
      nextSplitIndex,
      currentSectors,
      totalEnergyWh: current.totalEnergyWh + energyDeltaWh,
      totalEnergyOutWh: current.totalEnergyOutWh + energyOutDeltaWh,
      totalEnergyInWh: current.totalEnergyInWh + energyInDeltaWh,
      lapEnergyWh,
      lapEnergyOutWh,
      lapEnergyInWh,
      lapDistanceM,
      deltaRate,
      deltaMs,
    };
    liveStateRef.current = nextState;
    if (completedLap) {
      selectCompletedLapByDefault(completedLap.id);
      persistLiveSession(nextState, { saveLocal: true, saveBackend: true, includeFullLapSamples: false });
      lastLocalSessionSaveRef.current = Date.now();
      lastBackendSessionSaveRef.current = Date.now();
      lastSavedLapCountRef.current = nextState.laps.length;
    }
    setLiveState(nextState);
    return completedLap;
  }

  function triggerManualLap() {
    if (isMirrorRef.current) return;
    const current = liveStateRef.current;
    const sample = current.lastSample;
    if (!sample) {
      setLiveState((prev) => ({ ...prev, status: "Waiting for a live sample before starting a manual lap." }));
      return;
    }
    if (!current.lapStartMs) {
      const nextState = {
        ...current,
        lapStartMs: sample.t,
        sectorStartMs: sample.t,
        nextSplitIndex: 0,
        currentSectors: [],
        lapEnergyWh: 0,
        lapEnergyOutWh: 0,
        lapEnergyInWh: 0,
        lapDistanceM: 0,
        lapSamples: [sample],
        deltaMs: 0,
        status: "Manual lap started.",
      };
      liveStateRef.current = nextState;
      persistLiveSession(nextState, { saveLocal: true, saveBackend: true, includeFullLapSamples: false });
      lastLocalSessionSaveRef.current = Date.now();
      lastBackendSessionSaveRef.current = Date.now();
      setLiveState(nextState);
      return;
    }

    const durationMs = sample.t - current.lapStartMs;
    if (durationMs <= 0) {
      setLiveState((prev) => ({ ...prev, status: "Manual lap needs a positive time range." }));
      return;
    }

    const laps = [...current.laps];
    const completedLap: LiveLap = {
      id: `manual-lap-${laps.length + 1}-${sample.t}`,
      label: `Manual ${laps.length + 1}`,
      kind: "manual",
      startMs: current.lapStartMs,
      endMs: sample.t,
      durationMs,
      sectors: current.currentSectors,
      energyWh: current.lapEnergyWh,
      energyOutWh: current.lapEnergyOutWh,
      energyInWh: current.lapEnergyInWh,
      distanceM: current.lapDistanceM,
      avgSpeedMps: durationMs > 0 && current.lapDistanceM > 0 ? current.lapDistanceM / (durationMs / 1000) : null,
      samples: [],
    };
    laps.push(completedLap);
    const nextState = {
      ...current,
      laps,
      lapStartMs: sample.t,
      sectorStartMs: sample.t,
      nextSplitIndex: 0,
      currentSectors: [],
      lapEnergyWh: 0,
      lapEnergyOutWh: 0,
      lapEnergyInWh: 0,
      lapDistanceM: 0,
      lapSamples: [sample],
      deltaMs: 0,
      status: `${completedLap.label} logged.`,
    };
    liveStateRef.current = nextState;
    selectCompletedLapByDefault(completedLap.id);
    persistLiveSession(nextState, { saveLocal: true, saveBackend: true, includeFullLapSamples: false });
    lastLocalSessionSaveRef.current = Date.now();
    lastBackendSessionSaveRef.current = Date.now();
    lastSavedLapCountRef.current = nextState.laps.length;
    setLiveState(nextState);
  }

  // The single, coherent "a lap happened" action shared by the Live Viewer's
  // Log Lap button and the Dash tab's Send Lap button. It logs the lap in the
  // trackside viewer (source of truth) AND fires the driver's dash lap card when
  // a lap actually completes — so either tab does the whole thing. We bump
  // dashLastLapCountRef so the auto-send effect doesn't double-fire for this lap.
  function markLap() {
    if (isMirrorRef.current) return; // read-only mirror — leader owns laps
    const beforeLaps = liveStateRef.current.laps.length;
    const beforeStartMs = liveStateRef.current.lapStartMs;
    triggerManualLap();
    const after = liveStateRef.current.laps.length;
    const lapClosed = after > beforeLaps;
    // On a completed lap, always fire the car's lap card. sendLap advances the
    // lap counter in lockstep with the Live tab even if the publish is dropped
    // (link mid-handshake), and the auto-connect keeps the uplink up — so both
    // lap buttons reliably log to the Live tab AND reach the car.
    // Send the website's authoritative lap count to the car so dashd adopts it
    // (rather than running its own +1 counter that drifted from trackside).
    if (lapClosed) dashSignals.sendLap(after);
    dashLastLapCountRef.current = after;
    // Visible feedback so the user knows whether this click STARTED a lap (no
    // dash card yet — by design, the card represents 'lap COMPLETE') or CLOSED
    // a lap (dash card fires). Earlier this was silent, which made the two
    // identical buttons look inconsistent depending on which one closed vs
    // started a lap.
    if (lapClosed) {
      const dashOk = dashSignals.status === "connected";
      setMsgSendStatus(`Lap ${after} logged${dashOk ? " · lap card sent to driver ✓" : ` · dash uplink ${dashSignals.status}, card not sent.`}`);
    } else if (!beforeStartMs && liveStateRef.current.lapStartMs) {
      setMsgSendStatus(`Lap timer started — next click closes lap 1.`);
    }
    window.setTimeout(() => setMsgSendStatus(""), 4000);
  }

  function toggleLapSelected(lapId: string) {
    setSelectedLapIds((prev) => {
      const next = new Set(prev);
      if (next.has(lapId)) next.delete(lapId);
      else next.add(lapId);
      return next;
    });
  }

  function selectCompletedLapByDefault(lapId: string) {
    knownLapIdsRef.current.add(lapId);
    setSelectedLapIds((prev) => {
      if (prev.has(lapId)) return prev;
      const next = new Set(prev);
      next.add(lapId);
      return next;
    });
  }

  function isRestorableSession(saved: SavedSession | null | undefined): saved is SavedSession {
    // A session is restorable if there's ANY state worth restoring: a sessionInfo
    // (so the just-started session with no laps yet still comes back on reload,
    // along with its drive-day setup, plan, and starterId), or accumulated lap /
    // sample data. Earlier this required lap/sample data and would silently drop
    // a fresh session on reload — leaving the cone-flag row asking to "start a
    // session" again even though the user had just started one.
    return !!saved
      && saved.version === 1
      && (!!saved.sessionInfo || !!saved.laps?.length || !!saved.sampleTail?.length || !!saved.currentLap?.lapSamples?.length);
  }

  function selectedLapIdsForSave(laps: LiveLap[]) {
    const lapIds = new Set(laps.map((lap) => lap.id));
    const selected = new Set([...selectedLapIds].filter((id) => lapIds.has(id)));
    for (const lap of laps) {
      if (!knownLapIdsRef.current.has(lap.id)) selected.add(lap.id);
    }
    return [...selected];
  }

  function buildSavedSession(current: LiveSessionState, includeFullLapSamples: boolean): SavedSession {
    const currentLap: SavedCurrentLap | null = current.lapStartMs
      ? {
          lapStartMs: current.lapStartMs,
          sectorStartMs: current.sectorStartMs,
          nextSplitIndex: current.nextSplitIndex,
          currentSectors: current.currentSectors,
          lapEnergyWh: current.lapEnergyWh,
          lapEnergyOutWh: current.lapEnergyOutWh,
          lapEnergyInWh: current.lapEnergyInWh,
          lapDistanceM: current.lapDistanceM,
          lapSamples: current.lapSamples.slice(includeFullLapSamples ? 0 : -SESSION_AUTOSAVE_SAMPLE_CAP),
          deltaMs: current.deltaMs,
        }
      : null;
    return {
      version: 1,
      savedAt: Date.now(),
      source,
      topic: current.topic,
      targetLaps,
      targetEnergyKwh,
      soeCutoffCellV,
      totalEnergyWh: current.totalEnergyWh,
      totalEnergyOutWh: current.totalEnergyOutWh,
      totalEnergyInWh: current.totalEnergyInWh,
      laps: current.laps.map((lap) => ({ ...lap, samples: includeFullLapSamples ? lap.samples : [] })),
      selectedLapIds: selectedLapIdsForSave(current.laps),
      selectionSaved: true,
      sampleTail: current.samples.slice(includeFullLapSamples ? 0 : -SESSION_AUTOSAVE_SAMPLE_CAP),
      currentLap,
      hasSectors: sessionFromFile || current.laps.some((lap) => lap.sectors.length > 0),
      // Inject the LIVE metadata / plan / setup into sessionInfo so the saved
      // session always reflects the latest edits. patchSession only writes to
      // localStorage + server, not React state — without this merge,
      // downloadSessionJson and the autosave snapshot carry whatever values
      // were captured at session-create time (often empty setup).
      sessionInfo: sessionInfo
        ? {
            ...sessionInfo,
            metadata: { ...sessionInfo.metadata, ...metadataDraft },
            plan: { targetLaps, targetEnergyKwh, soeCutoffCellV },
            setup: { ...sessionInfo.setup, ...driveDaySetup } as Record<string, string | number | boolean>,
          }
        : null,
    };
  }

  // Merge the leader's shared session into a mirror's state, keeping this client's
  // own live gauges (lastSample/samples/feed status) untouched.
  function adoptSharedSession(saved: SavedSession) {
    const laps = (saved.laps ?? []).map(normalizeSavedLiveLap);
    const cur = saved.currentLap ?? null;
    const current = liveStateRef.current;
    const merged: LiveSessionState = {
      ...current,
      laps,
      lapStartMs: cur?.lapStartMs ?? null,
      sectorStartMs: cur?.sectorStartMs ?? null,
      nextSplitIndex: cur?.nextSplitIndex ?? 0,
      currentSectors: cur?.currentSectors ?? [],
      // Energy totals are integrated locally from this client's own feed (see
      // processLiveSample's mirror branch). Seed them from the leader the first
      // time (so a late-joining mirror starts at the running total) but never
      // overwrite a non-zero local total afterward — otherwise a leader on a
      // stale build that reports OUT=0 would keep zeroing our hero tiles.
      totalEnergyWh: current.totalEnergyWh !== 0 ? current.totalEnergyWh : (saved.totalEnergyWh ?? 0),
      totalEnergyOutWh: current.totalEnergyOutWh > 0 ? current.totalEnergyOutWh : (saved.totalEnergyOutWh ?? Math.max(0, saved.totalEnergyWh ?? 0)),
      totalEnergyInWh: current.totalEnergyInWh > 0 ? current.totalEnergyInWh : (saved.totalEnergyInWh ?? 0),
      lapEnergyWh: cur?.lapEnergyWh ?? 0,
      lapEnergyOutWh: cur?.lapEnergyOutWh ?? Math.max(0, cur?.lapEnergyWh ?? 0),
      lapEnergyInWh: cur?.lapEnergyInWh ?? 0,
      lapDistanceM: cur?.lapDistanceM ?? 0,
      deltaMs: cur?.deltaMs ?? 0,
    };
    knownLapIdsRef.current = new Set(laps.map((lap) => lap.id));
    liveStateRef.current = merged;
    setLiveState(merged);
    // NOTE: targetLaps / targetEnergyKwh / soeCutoffCellV are intentionally NOT
    // applied here — the race plan syncs through useRacePlan (the raceplan
    // endpoint), which is always fresh. Adopting them from this lagging
    // saved-session doc made the budget flip between the stale and live values.
    setSelectedLapIds(new Set(saved.selectedLapIds ?? []));
    if (saved.sessionInfo) setSessionInfo(saved.sessionInfo);
  }

  function persistLiveSession(
    current: LiveSessionState,
    {
      saveLocal,
      saveBackend,
      includeFullLapSamples,
    }: { saveLocal: boolean; saveBackend: boolean; includeFullLapSamples: boolean },
  ) {
    // Skip ONLY when there's truly nothing to persist. A fresh session with a
    // name + metadata + setup but no laps yet still deserves to be saved — the
    // restore path uses it to bring the session back on reload.
    if (!current.laps.length && !current.samples.length && !current.lapStartMs && !sessionInfo) return;
    const saved = buildSavedSession(current, includeFullLapSamples);
    if (saveLocal) {
      writeLocalSavedSession(saved);
      void writeIndexedSavedSession(saved);
    }
    // Only the leader owns the shared session doc — a mirror must never clobber it.
    if (saveBackend && !isMirrorRef.current) {
      const body = JSON.stringify(saved);
      // sendBeacon's queue survives a page-unload (regular fetch gets killed),
      // so it's the right tool for beforeunload/pagehide. Critical: the URL
      // must match the route — earlier this hit /api/live/session-cache
      // (missing /motec/) and silently 404'd EVERY save, so the server cache
      // never updated and sessions appeared to vanish when everyone left.
      const beaconSent = "sendBeacon" in navigator
        ? navigator.sendBeacon("/api/motec/live/session-cache", new Blob([body], { type: "application/json" }))
        : false;
      if (!beaconSent) void api.saveLiveSession(saved).catch(() => undefined);
    }
  }

  function restoreLiveState(saved: SavedSession, fromFile: boolean) {
    const laps = (saved.laps ?? []).map(normalizeSavedLiveLap);
    const currentLap = saved.currentLap ?? null;
    const tail = saved.sampleTail?.length ? saved.sampleTail : (currentLap?.lapSamples ?? []);
    const restored: LiveSessionState = {
      ...EMPTY_LIVE_STATE,
      status: fromFile ? "Loaded session from file." : "Restored saved session.",
      topic: saved.topic ?? "",
      startedAt: laps[0]?.startMs ?? currentLap?.lapStartMs ?? tail[0]?.t ?? null,
      lastSample: tail.at(-1) ?? null,
      previousSample: tail.length > 1 ? tail[tail.length - 2] : null,
      samples: tail,
      lapSamples: currentLap?.lapSamples ?? [],
      laps,
      lapStartMs: currentLap?.lapStartMs ?? null,
      sectorStartMs: currentLap?.sectorStartMs ?? null,
      nextSplitIndex: currentLap?.nextSplitIndex ?? 0,
      currentSectors: currentLap?.currentSectors ?? [],
      totalEnergyWh: saved.totalEnergyWh ?? 0,
      totalEnergyOutWh: saved.totalEnergyOutWh ?? Math.max(0, saved.totalEnergyWh ?? 0),
      totalEnergyInWh: saved.totalEnergyInWh ?? 0,
      lapEnergyWh: currentLap?.lapEnergyWh ?? 0,
      lapEnergyOutWh: currentLap?.lapEnergyOutWh ?? Math.max(0, currentLap?.lapEnergyWh ?? 0),
      lapEnergyInWh: currentLap?.lapEnergyInWh ?? 0,
      lapDistanceM: currentLap?.lapDistanceM ?? 0,
      deltaMs: currentLap?.deltaMs ?? 0,
    };
    knownLapIdsRef.current = new Set(laps.map((lap) => lap.id));
    liveStateRef.current = restored;
    lastSavedLapCountRef.current = laps.length;
    setLiveState(restored);
    if (saved.source === "orion" || saved.source === "angelique") setSource(saved.source);
    if (saved.topic) setLiveTopic(saved.topic);
    setTargetLaps(saved.targetLaps ?? 0);
    setTargetEnergyKwh(saved.targetEnergyKwh && saved.targetEnergyKwh > 0 ? saved.targetEnergyKwh : PACK_ENERGY_KWH);
    setSoeCutoffCellV(normalizeSavedSoeCutoffCellV(saved.soeCutoffCellV));
    setSelectedLapIds(defaultSelectedLapIds(laps, saved.selectedLapIds, saved.selectionSaved));
    setSessionFromFile(fromFile || !!saved.hasSectors);
    setResumeAvailable(null);
    setEventEnded(false);
  }

  function resumeSavedSession() {
    if (resumeAvailable) restoreLiveState(resumeAvailable, false);
  }

  function dismissSavedSession() {
    setResumeAvailable(null);
    removeLocalSavedSession();
    void removeIndexedSavedSession();
  }

  function saveSessionFile() {
    const current = liveStateRef.current;
    const saved = buildSavedSession(current, true);
    const blob = new Blob([JSON.stringify(saved)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    link.href = url;
    link.download = `${source}_session_${stamp}.json`;
    link.click();
    URL.revokeObjectURL(url);
  }

  function loadSessionFile(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const parsed = JSON.parse(String(reader.result)) as SavedSession;
        if (!parsed || parsed.version !== 1) throw new Error("Unrecognized session file format.");
        restoreLiveState(parsed, true);
      } catch (e) {
        showError(e);
      }
    };
    reader.readAsText(file);
  }

  async function downloadLiveLap(lap: LiveLap) {
    const current = liveStateRef.current;
    const lapSamples = lap.samples?.length ? lap.samples : current.samples.filter((sample) => sample.t >= lap.startMs && sample.t <= lap.endMs);
    if (lapSamples.length < 2 || lapSamples.at(-1)!.t <= lapSamples[0].t) {
      setLiveState((prev) => ({ ...prev, status: "Skipped live export because the completed lap did not have enough samples." }));
      return;
    }
    try {
      const response = await api.exportLiveLap({
        car: liveSourceNameRef.current,
        lap_label: lap.label,
        track_slug: liveTrackRef.current.slug || null,
        channel_chart_slug: liveChannelChartRef.current.slug || null,
        frequency_hz: 50,
        metadata: {
          ...liveMetadataRef.current,
          session: liveMetadataRef.current.session || lap.label,
          short_comment: liveMetadataRef.current.short_comment || "live-lap-latest",
        },
        samples: lapSamples.map((sample) => ({ t: sample.t, values: sample.values })),
      });
      const link = document.createElement("a");
      link.href = response.download_url;
      link.download = `${liveSourceNameRef.current}_live_latest.zip`;
      link.click();
      setExportUrl(response.download_url);
    } catch (e) {
      showError(e);
    }
  }

  // ── Manual recording: tag a start, stop, then export a MoTeC file of exactly
  //    that window. Works without track gates (unlike automatic lap export).
  function startRecording() {
    const suggested = `Recording ${new Date().toLocaleTimeString()}`;
    const name = window.prompt("Name this recording (used for the MoTeC session/file):", suggested);
    if (name === null) return; // cancelled
    const label = name.trim() || suggested;
    setRecordingLabel(label);
    setRecordingStartMs(Date.now());
    setLiveState((prev) => ({ ...prev, status: `Recording "${label}"…` }));
  }

  async function stopRecordingAndExport() {
    const startMs = recordingStartMs;
    setRecordingStartMs(null);
    if (startMs == null) return;
    const endMs = Date.now();
    const label = recordingLabel || `Recording ${new Date(startMs).toLocaleTimeString()}`;

    // Slice every buffered sample captured inside the record window.
    const windowSamples = liveStateRef.current.samples.filter((s) => s.t >= startMs && s.t <= endMs);
    if (windowSamples.length < 2 || windowSamples.at(-1)!.t <= windowSamples[0].t) {
      setLiveState((prev) => ({ ...prev, status: "Recording too short — no MoTeC file written." }));
      return;
    }

    setRecordingBusy(true);
    setLiveState((prev) => ({ ...prev, status: `Exporting "${label}" (${windowSamples.length} samples)…` }));
    try {
      const response = await api.exportLiveLap({
        car: liveSourceNameRef.current,
        lap_label: label,
        track_slug: liveTrackRef.current.slug || null,
        channel_chart_slug: liveChannelChartRef.current.slug || null,
        frequency_hz: 50,
        metadata: {
          ...liveMetadataRef.current,
          session: liveMetadataRef.current.session || label,
          short_comment: liveMetadataRef.current.short_comment || "manual-recording",
        },
        samples: windowSamples.map((s) => ({ t: s.t, values: s.values })),
      });
      const durationS = ((endMs - startMs) / 1000).toFixed(1);
      const link = document.createElement("a");
      link.href = response.download_url;
      link.download = `${liveSourceNameRef.current}_${safeFileLabel(label)}.zip`;
      link.click();
      setExportUrl(response.download_url);
      setLiveState((prev) => ({ ...prev, status: `Saved "${label}" MoTeC export (${durationS}s).` }));
    } catch (e) {
      showError(e);
    } finally {
      setRecordingBusy(false);
    }
  }

  async function exportSelection() {
    if (!exportSegments.length) return;
    setBusy(true);
    setError("");
    try {
      const response = await api.export({
        car: source,
        channel_keys: channels.map((c) => c.key),
        segments: exportSegments,
        export_type: exportType,
        frequency_hz: 50,
        track_slug: exportType === "motec" && track.gates.length ? track.slug : null,
        channel_chart_slug: channelChart.slug || null,
        metadata: {
          ...metadataDraft,
          vehicle_id: metadataDraft.vehicle_id || sourceLabel,
          event: metadataDraft.event || `${sourceLabel} Telemetry Export`,
          session: metadataDraft.session || selectedDate,
        },
      });
      setExportUrl(`/api/motec/export/download?id=${encodeURIComponent(response.export_id)}`);
    } catch (e) {
      showError(e);
    } finally {
      setBusy(false);
    }
  }

  return (
    <main className="shell">
      {showTour ? (
        <TracksideTour
          onNavigate={(tab) => setActiveTab(tab)}
          onClose={() => dismissTour(true)}
          onFinish={() => dismissTour(true)}
        />
      ) : null}
      {newSessionOpen ? (
        <NewSessionModal
          sources={sources}
          defaultCar={source}
          defaultVenue={track.name}
          defaultDriver={metadataDraft.driver}
          onStart={startNewSession}
          onClose={() => setNewSessionOpen(false)}
        />
      ) : null}
      {setupOpen ? (
        <DriveDaySetupForm
          value={driveDaySetup}
          disabled={isMirror}
          onChange={(field, v) => setDriveDaySetup((prev) => ({ ...prev, [field]: v }))}
          onClose={() => setSetupOpen(false)}
        />
      ) : null}
      {lapDesignerOpen ? (
        <DashLayoutEditor onClose={() => setLapDesignerOpen(false)} onSend={sendLapLayout} sendStatus={layoutSendStatus} />
      ) : null}
      {confirmStopOpen ? (
        <div className="modalOverlay" onMouseDown={() => setConfirmStopOpen(false)}>
          <div className="modalCard" style={{ maxWidth: 460 }} onMouseDown={(e) => e.stopPropagation()}>
            <div className="modalHead">
              <h3>Stop event?</h3>
              <button className="tool iconOnly" aria-label="Close" onClick={() => setConfirmStopOpen(false)}><X size={16} /></button>
            </div>
            <p className="muted" style={{ marginTop: 0 }}>
              Downloads <strong>{liveState.laps.length} lap{liveState.laps.length === 1 ? "" : "s"}</strong> + the full session JSON to your machine,
              then clears the on-screen laps and gauges so the next session starts fresh.
              Raw car CSVs are still available via Log Sync.
            </p>
            <div style={{ display: "flex", gap: 8, justifyContent: "flex-end", marginTop: "var(--s-3)" }}>
              <button className="tool" onClick={() => setConfirmStopOpen(false)}>Cancel</button>
              <button className="primary dangerPrimary" onClick={() => { setConfirmStopOpen(false); stopEventAndDownload(); }}>
                <Power size={14} /> Stop event
              </button>
            </div>
          </div>
        </div>
      ) : null}
      {flagScreensOpen ? (
        <div className="modalOverlay" onMouseDown={() => setFlagScreensOpen(false)}>
          <div className="modalCard" style={{ maxWidth: 480 }} onMouseDown={(e) => e.stopPropagation()}>
            <div className="modalHead">
              <h3>Driver-dash screen per flag</h3>
              <button className="tool iconOnly" aria-label="Close" onClick={() => setFlagScreensOpen(false)}><X size={16} /></button>
            </div>
            <p className="muted" style={{ marginTop: 0 }}>
              When “Show flags on driver dash” is on, each flag flashes the screen you pick here.
              “Auto” builds one from the flag name; “Don’t send” logs the flag without showing anything.
            </p>
            <div className="flagScreenRows">
              {["Hit Cone", "Off-track", "Incomplete"].map((fl) => (
                <label key={fl} className="flagScreenRow">
                  <span>{fl}</span>
                  <select
                    value={flagMessageMap[fl] ?? "auto"}
                    disabled={isMirror}
                    onChange={(e) => setFlagMessageMap((p) => ({ ...p, [fl]: e.target.value }))}
                  >
                    <option value="auto">Auto (flag text)</option>
                    <option value="none">Don&apos;t send</option>
                    {msgLib.lib.items.map((m) => (
                      <option key={m.id} value={m.id}>{m.label}</option>
                    ))}
                  </select>
                </label>
              ))}
            </div>
            <small className="muted">Custom “Flag…” always uses its typed text. Create/edit screens in Dash tab → Messages.</small>
            <div style={{ display: "flex", justifyContent: "flex-end", marginTop: "var(--s-3)" }}>
              <button className="primary" onClick={() => setFlagScreensOpen(false)}>Done</button>
            </div>
          </div>
        </div>
      ) : null}
      {msgEditorOpen ? (
        <DashMessageEditor
          api={msgLib}
          onClose={() => setMsgEditorOpen(false)}
          onSendTest={sendDriverMessage}
          sendStatus={msgSendStatus}
          canSend={!isMirror && dashSignals.status === "connected"}
        />
      ) : null}
      <header className="topbar">
        <div>
          <h1>Trackside Live</h1>
          <p>Live telemetry, lap timing &amp; energy strategy · MoTeC / CSV export · track builder</p>
        </div>
        <div className="topActions">
          <span
            className={`clientsBadge${presence.clients >= presence.max ? " atLimit" : ""}`}
            title={
              `${presence.clients} of ${presence.max} client${presence.clients === 1 ? "" : "s"} connected` +
              (presence.role === "leader" ? " · you are the session leader" : presence.isMirror ? " · you are a read-only mirror" : "") +
              (presence.clients >= presence.max ? " · limit reached" : "")
            }
          >
            <Users size={14} /> {presence.clients}/{presence.max}
          </span>
          {carState ? (
            <span className={`carStateBadge ${CAR_STATE_META[carState].cls}`} title="Car state from the on-stack classifier">
              <span className="stateDot" /> {CAR_STATE_META[carState].label}
            </span>
          ) : null}
          <div
            className="status"
            title={`${sourceLabel} · telemetry ${uplinkMeta.label.toLowerCase()}${carState ? ` · car ${CAR_STATE_META[carState].label}` : ""} · database ${health?.postgres_enabled ? "online" : "offline"}${busy ? " · syncing" : ""}`}
          >
            <span className={`dot ${busy ? "stale pulse" : uplinkMeta.dot}`} />
            <span>{busy ? "Syncing…" : uplinkMeta.label}</span>
            <span className="statusSep">·</span>
            <span className="statusSrc">{sourceLabel}</span>
          </div>
          <button className="tool iconOnly" onClick={() => setShowTour(true)} aria-label="Show guided tour" title="Guided tour">
            <HelpCircle size={16} />
          </button>
          <button className="tool iconOnly" onClick={() => setDarkMode((value) => !value)} aria-label="Toggle dark mode" title="Toggle theme">
            {darkMode ? <Sun size={16} /> : <Moon size={16} />}
          </button>
        </div>
      </header>

      {error ? (
        <div className="error" role="alert">
          <span>{error}</span>
          <button type="button" className="errorDismiss" aria-label="Dismiss error" onClick={() => setError("")}>
            <X size={16} />
          </button>
        </div>
      ) : null}

      {isMirror ? (
        <div className="syncBanner mirror" role="status">
          <Radio size={15} />
          <span>
            {presence.role === "full"
              ? `Viewer limit reached (${presence.max}) — read-only mirror.`
              : "Mirroring the session leader — read-only."}{" "}
            Live gauges are your own feed; laps &amp; strategy follow the leader.
          </span>
          <input className="nameField" value={presence.name} placeholder="your name"
            onChange={(e) => presence.setName(e.target.value)} aria-label="Your name" />
          {isAdmin ? (
            <button className="primary" style={{ whiteSpace: "nowrap" }} title="Admin: seize control immediately" onClick={() => presence.forceLeader()}>
              <Power size={14} /> Take control
            </button>
          ) : presence.role !== "full" ? (
            presence.requested
              ? <span className="goodText" style={{ whiteSpace: "nowrap" }}>Control requested…</span>
              : <button className="tool" style={{ whiteSpace: "nowrap" }} onClick={() => presence.requestLeader()}>Request control</button>
          ) : null}
          <span className="syncCount">{presence.clients}/{presence.max} clients</span>
        </div>
      ) : presence.clients > 1 ? (
        <div className="syncBanner leader" role="status">
          <Radio size={15} />
          <span>You&apos;re the session leader — your laps &amp; strategy sync to {presence.clients - 1} mirror{presence.clients - 1 === 1 ? "" : "s"}.</span>
          <input className="nameField" value={presence.name} placeholder="your name"
            onChange={(e) => presence.setName(e.target.value)} aria-label="Your name" />
          <span className="syncCount">{presence.clients}/{presence.max} clients</span>
        </div>
      ) : null}

      {/* Leader sees control-transfer requests from mirrors. */}
      {!isMirror && presence.requests.length ? (
        <div className="modalOverlay" onMouseDown={() => presence.requests.forEach((r) => presence.denyLeader(r.id))}>
          <div className="modalCard" style={{ maxWidth: 420 }} onMouseDown={(e) => e.stopPropagation()}>
            <div className="modalHead"><h3>Control request</h3></div>
            <p className="muted" style={{ marginTop: 0 }}>
              {presence.requests.length === 1 ? "A client is requesting" : `${presence.requests.length} clients are requesting`} session control. Transfer makes them the leader and you a read-only mirror.
            </p>
            {presence.requests.map((r) => (
              <div key={r.id} className="requestRow">
                <span className="reqId">{r.name?.trim() || `viewer …${r.id.slice(-4)}`}</span>
                <div style={{ display: "flex", gap: 8 }}>
                  <button className="tool" onClick={() => presence.denyLeader(r.id)}>Deny</button>
                  <button className="primary" onClick={() => presence.grantLeader(r.id)}><Power size={14} /> Transfer</button>
                </div>
              </div>
            ))}
          </div>
        </div>
      ) : null}

      {/* Trackside-focused tabs only. Exporter + Race Ops are kept in the source
          (their sections still render if activeTab is set to them) but the buttons
          are hidden for now — re-add a button here to bring either back. */}
      <nav className="tabBar" aria-label="Telemetry workspace">
        <button className={activeTab === "live" ? "tab activeTab" : "tab"} onClick={() => setActiveTab("live")}>
          <Radio size={16} /> Live Viewer
        </button>
        <button className={activeTab === "track-builder" ? "tab activeTab" : "tab"} onClick={() => setActiveTab("track-builder")}>
          <MapPinned size={16} /> Track Builder
        </button>
        <button className={activeTab === "dash" ? "tab activeTab" : "tab"} onClick={() => setActiveTab("dash")}>
          <Gauge size={16} /> Dash
        </button>
      </nav>

      {activeTab === "exporter" ? <section className="grid">
        <aside className="sidebar">
          <Panel title="Days" icon={<CalendarDays size={18} />}>
            <div className="sourceRow">
              <select value={source} onChange={(e) => setSource(e.target.value as "orion" | "angelique")}>
                {sources.map((item) => (
                  <option key={item.key} value={item.key}>{item.label}</option>
                ))}
              </select>
              <button className="tool iconOnly" onClick={() => refreshCalendar()} aria-label="Refresh"><RefreshCcw size={16} /></button>
            </div>
            <div className="dayList">
              {days.map((day) => (
                <button
                  key={day.date}
                  className={day.date === selectedDate ? "day selected" : "day"}
                  onClick={() => setSelectedDate(day.date)}
                >
                  <strong>{day.date}</strong>
                  <span>
                    {threshold <= 0 && sessionCountsByDate[day.date] == null
                      ? `${day.sessions} source range${day.sessions === 1 ? "" : "s"}`
                      : sessionCountsByDate[day.date] == null
                      ? "valid"
                      : `${sessionCountsByDate[day.date]} session${sessionCountsByDate[day.date] === 1 ? "" : "s"}`}
                  </span>
                </button>
              ))}
              {!days.length ? <small className="muted">No days match the current threshold and minimum length.</small> : null}
            </div>
          </Panel>

          <Panel title="Sessions" icon={<Scissors size={18} />}>
            <div className="stack compact">
              {(() => {
                const MIN_TRACE_GPS_POINTS = 30;
                const visibleSegments = gpsOnlySessions
                  ? segments.filter((s) => s.has_gps && s.gps_points >= MIN_TRACE_GPS_POINTS)
                  : segments;
                const hiddenCount = segments.length - visibleSegments.length;
                return (
              <>
              <label className="checkInline" style={{ marginBottom: 4 }}>
                <input type="checkbox" checked={gpsOnlySessions} onChange={(e) => setGpsOnlySessions(e.target.checked)} />
                GPS sessions only{hiddenCount > 0 ? ` (${hiddenCount} hidden)` : ""}
              </label>
              {sourceRanges.length ? (
                <small className="muted">
                  Threshold sessions for selected local day
                </small>
              ) : null}
              <div className="sessionSummary">
                <strong>{previewSegments.length ? `${previewSegments.length} previewed` : "No preview selected"}</strong>
                <span>
                  {previewSummary
                    ? `${formatTime(previewSummary.startMs)} - ${formatDuration(previewSummary.durationS)} total`
                    : "Click a session row to load its graph, GPS, and metadata."}
                </span>
                {visibleSegments.length > 1 ? <button className="textButton" onClick={selectAllSessionsForPreview}>Preview all</button> : null}
              </div>
              {visibleSegments.map((segment) => (
                <div
                  key={segment.id}
                  className={previewSelectedSegments.has(segment.id) ? "sessionRow previewSelected" : "sessionRow"}
                  role="button"
                  tabIndex={0}
                  onClick={(event) => handleSessionClick(event, segment)}
                  onKeyDown={(event) => {
                    if (event.key === "Enter" || event.key === " ") {
                      event.preventDefault();
                      setRange(null);
                      setPreviewSelectedSegments(new Set([segment.id]));
                      setLastPreviewSegmentId(segment.id);
                    }
                  }}
                >
                  <input
                    type="checkbox"
                    checked={selectedSegments.has(segment.id) && !range}
                    onClick={(event) => event.stopPropagation()}
                    onChange={() => toggleSegment(segment)}
                    aria-label={`Export ${segment.label}`}
                  />
                  <span>
                    <span className="sessionTitle">
                      <strong>{segment.label}</strong>
                      <SessionGpsBadge segment={segment} />
                    </span>
                    <small>{formatTime(segment.start_ms)} - {formatDuration(segment.duration_s)}</small>
                  </span>
                </div>
              ))}
              {!visibleSegments.length ? (
                <small className="muted">
                  {segments.length
                    ? `All ${segments.length} session${segments.length === 1 ? "" : "s"} hidden — none have a usable GPS trace. Uncheck “GPS sessions only” to show them.`
                    : "No threshold sessions in this day."}
                </small>
              ) : null}
              </>
                );
              })()}
            </div>
          </Panel>
        </aside>

        <section className="workspace">
          <div className="toolbar">
            <label className="channelPicker">
              <Gauge size={16} />
              <select value={channel} onChange={(e) => setChannel(e.target.value)}>
                {sortedChannels.map((c) => (
                  <option key={c.key} value={c.key}>{c.label}</option>
                ))}
              </select>
            </label>
            <label className="thresholdBox">
              <SlidersHorizontal size={16} />
              <span>Threshold</span>
              <input type="number" value={threshold} onChange={(e) => setThreshold(Number(e.target.value))} />
            </label>
            <label className="thresholdBox smallInput">
              <span>Min Length</span>
              <input type="number" min={0} step={1} value={minDurationS} onChange={(e) => setMinDurationS(Math.max(0, Number(e.target.value)))} />
              <span>s</span>
            </label>
            <label className="thresholdBox exportTypeBox">
              <span>Export</span>
              <select value={exportType} onChange={(e) => setExportType(e.target.value as "motec" | "csv")}>
                <option value="motec">MoTeC</option>
                <option value="csv">CSV</option>
              </select>
            </label>
            <label className="thresholdBox trackPicker">
              <Flag size={16} />
              <select
                value={track.slug}
                onChange={(e) => setTrack(normalizeTrack(tracks.find((t) => t.slug === e.target.value) ?? track))}
              >
                <option value={track.slug}>{track.name}</option>
                {tracks.filter((t) => t.slug !== track.slug).map((t) => (
                  <option key={t.slug} value={t.slug}>{t.name}</option>
                ))}
              </select>
            </label>
            <button className="primary" disabled={!exportSegments.length || busy} onClick={exportSelection}>
              <Download size={16} /> Export {exportType === "csv" ? "CSV" : "MoTeC"} {range ? "range" : exportSegments.length || ""}
            </button>
            {exportUrl ? <a className="download" href={exportUrl}>Download ZIP</a> : null}
          </div>

          <Panel title={selectedChannel?.label || "Channel"} icon={<Gauge size={18} />}>
            <TelemetryChart
              points={series}
              unit={selectedChannel?.unit || ""}
              range={range}
              segments={segments}
              previewSegmentIds={previewSelectedSegments}
              threshold={threshold}
              onRange={setRange}
            />
            <div className="rangeBar">
              <span>{range ? `${formatDateTime(range[0])} to ${formatDateTime(range[1])}` : "The plot follows clicked sessions. Drag across it only when you need a one-off manual export range."}</span>
              {range ? <button className="tool" onClick={() => setRange(null)}>Use Checked Sessions</button> : null}
            </div>
          </Panel>

          <div className="lower">
            <Panel title="GPS Trace" icon={<MapPinned size={18} />} className="gpsPanel">
              <GpsTrace
                points={gps}
                gates={track.gates}
                drawMode={null}
                onDrawGate={handleDrawGate}
                nextSplitNumber={track.gates.filter((gate) => gate.role === "split").length + 1}
              />
              <LapPreview rows={lapPreview} hasStartFinish={hasStartFinish} gpsPointCount={gps.length} />
            </Panel>

            <div className="rightRail">
              <Panel title="MoTeC Metadata" icon={<FileText size={18} />}>
                <div className="metadataStatus">
                  <strong>{previewSegments.length ? `${previewSegments.length} selected` : "No session selected"}</strong>
                  <span>{mixedMetadata.size ? `${mixedMetadata.size} mixed fields` : "Fields match across selection"}</span>
                </div>
                <MetadataFields
                  values={metadataDraft}
                  mixed={mixedMetadata}
                  onChange={(key, value) => setMetadataDraft((prev) => ({ ...prev, [key]: value }))}
                />
                <button className="primary fullWidth" disabled={!previewSegments.length} onClick={applyMetadata}>
                  <Save size={16} /> Apply Metadata
                </button>
              </Panel>

              <Panel title="Channel Chart" icon={<Gauge size={18} />}>
                <div className="trackForm">
                  <input value={channelChart.name} onChange={(e) => setChannelChart({ ...channelChart, name: e.target.value })} />
                  <select
                    value={channelChart.slug}
                    onChange={(e) => setChannelChart(channelCharts.find((chart) => chart.slug === e.target.value) ?? channelChart)}
                  >
                    <option value={channelChart.slug}>{channelChart.name}</option>
                    {channelCharts.filter((chart) => chart.slug !== channelChart.slug).map((chart) => (
                      <option key={chart.slug} value={chart.slug}>{chart.name}</option>
                    ))}
                  </select>
                  <textarea value={channelChart.notes} placeholder="Chart notes" onChange={(e) => setChannelChart({ ...channelChart, notes: e.target.value })} />
                </div>
                <div className="chartSummary">
                  <strong>{channelChart.entries.length}</strong>
                  <span>channel metadata rows</span>
                </div>
                <div className="gateButtons">
                  <button className="tool" onClick={() => saveChannelChart()}><Save size={15} /> Save</button>
                  <label className="tool fileTool">
                    <Upload size={15} /> Upload
                    <input type="file" accept="application/json,.json,text/csv,.csv" onChange={uploadChannelChart} />
                  </label>
                  <button className="tool" onClick={downloadChannelChart}><Download size={15} /> Download</button>
                </div>
              </Panel>

            </div>
          </div>
        </section>
      </section> : null}

      {activeTab === "live" ? (
        <section className="liveShell">
          {resumeAvailable ? (
            <div className="resumeBanner">
              <span>
                Saved session found ({resumeAvailable.laps?.length ?? 0} laps, saved {new Date(resumeAvailable.savedAt).toLocaleString()}).
              </span>
              <div className="resumeBannerActions">
                <button className="tool" onClick={resumeSavedSession}>Resume</button>
                <button className="tool" onClick={dismissSavedSession}>Dismiss</button>
              </div>
            </div>
          ) : null}
          {/* Session-starter reclaim banner: the user who started this session
              is back, but another client became leader while they were gone
              (their TTL expired). One-click reclaim instead of having to wait
              on the current leader to grant a transfer. */}
          {isMirror && presence.clientId && sessionInfo?.starterId === presence.clientId && !starterReclaimDismissed ? (
            <div className="resumeBanner">
              <span>
                You started <strong>{sessionInfo.name}</strong> — take back control?
              </span>
              <div className="resumeBannerActions">
                <button className="primary" onClick={() => { presence.claimAsStarter(); setStarterReclaimDismissed(true); }}>
                  <Power size={14} /> Take Over
                </button>
                <button className="tool" onClick={() => setStarterReclaimDismissed(true)}>Stay as Mirror</button>
              </div>
            </div>
          ) : null}
          <div className="liveHero">
            <div className="liveHeroMain">
              <div className="carStatusStrip">
                {carState ? (
                  <span className={`carStateBadge ${CAR_STATE_META[carState].cls}`} title="Car state from the on-stack classifier">
                    <span className="stateDot" /> {CAR_STATE_META[carState].label}
                  </span>
                ) : (
                  <span className={liveBadgeClass}><Radio size={14} /> {liveBadgeLabel}</span>
                )}
                <span className="freshTag"><span className={`dot ${uplinkMeta.dot}`} /> {uplinkMeta.label}</span>
                <span className="freshTag" title={sfGateDefined ? "GPS auto-lap detection is active and backs up the manual Log Lap button" : "Define a start/finish gate in Track Builder to enable GPS auto-lap"}>
                  <span className={`dot ${gpsTag.dot}`} /> {gpsTag.label}
                </span>
                {(() => {
                  // "Dash linked" needs BOTH: broker connection AND fresh state
                  // from the car's dashd (lhre/dash/state publishes at ~2 Hz).
                  // Stale lastStateAt → broker is up but car is off / dashd is
                  // down — show that distinctly so a green pill can't lie about
                  // an unreachable car.
                  const carFresh = dashSignals.lastStateAt != null && (liveNowMs - dashSignals.lastStateAt) < 6000;
                  const connected = dashSignals.status === "connected";
                  const linkLive = connected && carFresh;
                  const label = linkLive ? "Dash linked"
                    : connected ? "Dash silent"
                    : dashSignals.status === "connecting" ? "Dash linking…"
                    : "Dash off";
                  // Broker-up-but-no-car-state and broker-unreachable both mean
                  // the dash is unreachable from a user perspective → both red.
                  // Yellow is reserved for genuinely in-between (connecting).
                  const dot = linkLive ? "live"
                    : dashSignals.status === "connecting" ? "stale pulse"
                    : "dead";
                  const title = isMirror ? "Read-only mirror — the leader holds the dash link"
                    : linkLive ? "Dash uplink connected and the car's dashd is publishing state — lap cards & driver messages reach the car"
                    : connected ? "Broker reachable but no recent state from the car's dashd (car off / dashd down). Lap cards & messages won't be seen."
                    : dashSignals.status === "connecting" ? "Dash uplink connecting…"
                    : "Dash uplink offline — broker unreachable";
                  return (
                    <span className="freshTag" title={title}>
                      <span className={`dot ${dot}`} /> {label}
                    </span>
                  );
                })()}
              </div>
              <h2>{liveState.lapStartMs ? formatLapTime(liveLapElapsedMs) : "0:00.00"}</h2>
              <p>{eventEnded ? "Event ended — start a new session to run again" : liveState.lapStartMs ? "Flying lap" : "Out lap / waiting for start"}</p>
              <DeltaBar rate={liveState.deltaRate} totalMs={liveState.deltaMs} />
              <div className="heroInputs">
                <div className="heroInputBar">
                  <span>THR</span>
                  <div className="barTrack"><div className="barFill thr" style={{ width: `${Math.max(0, Math.min(100, heroControls.throttlePercent ?? 0))}%` }} /></div>
                  <b>{heroControls.throttlePercent == null ? "--" : `${heroControls.throttlePercent.toFixed(0)}%`}</b>
                </div>
                <div className="heroInputBar">
                  <span>BRK</span>
                  <div className="barTrack"><div className="barFill brk" style={{ width: `${Math.max(0, Math.min(100, ((heroControls.bse1Psi ?? 0) / 1500) * 100))}%` }} /></div>
                  <b>{heroControls.bse1Psi == null ? "--" : `${heroControls.bse1Psi.toFixed(0)} psi`}</b>
                </div>
                <div className="heroSteer"><span>STR</span><b>{heroControls.steeringAngleDeg == null ? "--" : `${heroControls.steeringAngleDeg.toFixed(0)}°`}</b></div>
              </div>
              {!isMirror ? (() => {
                const msgs = activeMessages(msgLib.lib);
                const linked = dashSignals.status === "connected";
                if (!msgs.length) return null;
                return (
                  <div className="heroMessages">
                    {msgs.map((m) => (
                      <button
                        key={m.id}
                        type="button"
                        className="heroMsgBtn"
                        disabled={!linked}
                        title={linked ? `Send “${m.text}” to the driver${m.durationS ? ` (${m.durationS}s)` : " (until cleared)"}` : "Dash not connected — can't reach the driver"}
                        onClick={() => sendDriverMessage(m)}
                      >
                        <span aria-hidden>{MESSAGE_ICON_GLYPH[m.icon]}</span> {m.label}
                      </button>
                    ))}
                    <button
                      type="button"
                      className="heroMsgBtn heroMsgClear"
                      disabled={!linked}
                      title={linked ? "Clear the message currently showing on the driver's dash" : "Dash not connected — can't clear"}
                      onClick={clearDriverMessage}
                    >
                      <X size={13} /> Clear
                    </button>
                  </div>
                );
              })() : null}
            </div>
            <div className="liveHeroStats">
              {/* Tier 1 race-state metrics — biggest, glanced every second. */}
              <div className="heroMetricRow">
                <Metric label="Best" value={bestLap ? formatLapTime(bestLap.durationMs) : "--"} tone="purple" />
                <Metric label="Lap" value={`${currentLapNumber} / ${targetLaps > 0 ? targetLaps : "—"}`} />
                <Metric label="Energy" value={`${liveState.totalEnergyOutWh.toFixed(1)} Wh`} />
              </div>
              {/* Tier 2 — compact inline strip, smaller because they're checked
                  every few seconds, not every second. */}
              <div className="heroMetricStrip">
                <span className="heroMetricInline">
                  <small>Regen In</small>
                  <b className={liveState.totalEnergyInWh > 0 ? "goodText" : ""}>{liveState.totalEnergyInWh.toFixed(1)} Wh</b>
                </span>
                <span className="heroMetricInline">
                  <small>Torque</small>
                  <b className={liveTorqueNm != null && liveTorqueNm < 0 ? "goodText" : ""}>
                    {liveTorqueNm == null ? "--" : formatSignedTorque(liveTorqueNm)}
                  </b>
                </span>
              </div>
              {/* Tier 3 — incident flags. Land in the classifier against the
                  active drive_day (cone hits, off-track, DNF, custom). Disabled
                  until a session is active (no drive_day to attach to) — when
                  that's the case, show the reason inline instead of mystery-
                  greying the row. */}
              {!sessionInfo && !isMirror ? (
                <div className="heroFlagsHint">
                  <span>Flags need an active session.</span>
                  <button type="button" className="tool" onClick={() => setNewSessionOpen(true)}>
                    <Plus size={13} /> Start a session
                  </button>
                </div>
              ) : (
                <div className="gateButtons heroFlags">
                  <button className="tool" disabled={isMirror || !sessionInfo} title={isMirror ? "Read-only mirror — the leader logs flags" : "Flag a cone hit"} onClick={() => void eventFlag("Hit Cone")}>
                    <span style={{ color: "#f5a524" }}>●</span> Cone
                  </button>
                  <button className="tool" disabled={isMirror || !sessionInfo} title={isMirror ? "Read-only mirror — the leader logs flags" : "Flag going off-track"} onClick={() => void eventFlag("Off-track")}>
                    <span style={{ color: "#ff4d4f" }}>●</span> Off-track
                  </button>
                  <button className="tool" disabled={isMirror || !sessionInfo} title={isMirror ? "Read-only mirror — the leader logs flags" : "Flag an incomplete / DNF run"} onClick={() => void eventFlag("Incomplete")}>
                    <AlertTriangle size={14} /> Incomplete
                  </button>
                  <button className="tool" disabled={isMirror || !sessionInfo} title={isMirror ? "Read-only mirror — the leader logs flags" : "Custom event flag"} onClick={customFlag}>
                    <Flag size={14} /> Flag…
                  </button>
                  {/* Clear the message currently on the driver's dash. Useful
                      after firing a flag with a long-duration screen, or to
                      dismiss a stale message left over from a prior flag. */}
                  <button
                    type="button"
                    className="tool flagClearBtn"
                    disabled={isMirror || dashSignals.status !== "connected"}
                    title={isMirror ? "Read-only mirror" : dashSignals.status !== "connected" ? "Dash uplink not connected" : "Clear the message currently on the driver's dash"}
                    onClick={clearDriverMessage}
                  >
                    <X size={13} /> Clear
                  </button>
                </div>
              )}
              {msgSendStatus ? (
                <small className="heroFlagStatus" role="status">{msgSendStatus}</small>
              ) : null}
              {/* Tier 4 — set-and-forget toggles. Compact options row. */}
              <div className="liveControlOpts">
                <label className="checkInline" title="When on, each flag above also flashes a message on the driver's dash (needs the dash link connected).">
                  <input type="checkbox" checked={flagSendsMessage} disabled={isMirror} onChange={(e) => setFlagSendsMessage(e.target.checked)} />
                  Show on dash{flagSendsMessage && dashSignals.status !== "connected" ? " ⚠" : ""}
                </label>
                {flagSendsMessage ? (
                  <button type="button" className="tool tinyTool" disabled={isMirror} onClick={() => setFlagScreensOpen(true)} title="Choose which dash screen each flag fires">
                    <SlidersHorizontal size={13} /> Flag screens…
                  </button>
                ) : null}
                <label className="checkInline">
                  <input type="checkbox" checked={autoLiveDownload} onChange={(e) => setAutoLiveDownload(e.target.checked)} />
                  Auto MoTeC / lap
                </label>
              </div>
            </div>
            <div className="liveControls">
              {/* The three primary trackside actions. Tier 1 (Log/Start Lap is
                  primary; Record + Stop are paired Tier 3 captures). Everything
                  else moved into the stats column so this stays short. */}
              <button
                className="primary liveAction"
                disabled={isMirror || !telemetryFresh}
                title={isMirror ? "Read-only mirror — the leader logs laps" : !telemetryFresh ? "No live telemetry from the car" : "Logs the lap here on the Live tab and fires the driver's dash lap card on the car — same button as the Dash tab"}
                onClick={markLap}
              >
                <Flag size={20} /> {liveState.lapStartMs ? "Log Lap" : "Start Lap"}
              </button>
              <div className="liveActionPair">
                <button
                  className={recordingStartMs != null ? "tool liveAction dangerPrimary" : "tool liveAction"}
                  disabled={isMirror || recordingBusy || (recordingStartMs == null && !telemetryFresh)}
                  onClick={recordingStartMs != null ? stopRecordingAndExport : startRecording}
                  title={recordingStartMs != null ? "Stop & save this recording" : isMirror ? "Read-only mirror" : !telemetryFresh ? "No live telemetry from the car" : "Tag a start, then stop to download a MoTeC file of exactly that window"}
                >
                  <Disc3 size={20} />{" "}
                  {recordingBusy
                    ? "Exporting…"
                    : recordingStartMs != null
                      ? `Stop (${formatLapTime(recordingElapsedMs)})`
                      : "Record"}
                </button>
                <button
                  className="tool liveAction"
                  disabled={isMirror || eventEnded || (!liveState.laps.length && !liveState.samples.length)}
                  onClick={() => setConfirmStopOpen(true)}
                  title={eventEnded ? "Event already stopped — start a new session to run again" : "End the session and download everything to review it locally (laps CSV + full session JSON). Raw car CSVs come from Log Sync."}
                >
                  <Power size={20} /> {eventEnded ? "Stopped" : "Stop Event"}
                </button>
              </div>
              <small className="muted">{liveState.status}</small>
            </div>
          </div>

          <div className="liveLayout">
            <div className="leftRail">
              <div id="car-status-anchor">
              <CarStatusPanel
                feed={carStatus}
                carState={carState}
                stale={carStateStale}
                ageMs={carStateAgeMs}
                uplinkLabel={uplinkMeta.label}
                uplinkDot={uplinkMeta.dot}
                running={liveState.running}
              />
              </div>
              <Panel title="Live Laps" icon={<Timer size={18} />}>
                <LiveLapTable
                  laps={liveState.laps}
                  bestLap={bestLap}
                  bestSectors={bestSectors}
                  currentSectors={liveState.currentSectors}
                  currentLapElapsedMs={liveLapElapsedMs}
                  currentLapEnergyWh={liveState.lapEnergyWh}
                  currentLapEnergyOutWh={liveState.lapEnergyOutWh}
                  currentLapEnergyInWh={liveState.lapEnergyInWh}
                  sectorCount={liveSectorCount}
                  selectedLapIds={selectedLapIds}
                  onToggleLap={toggleLapSelected}
                  onEditLapTime={editLapDuration}
                  onEditLapNote={editLapNote}
                  onDeleteLap={deleteLap}
                  editable={!isMirror}
                  targetEnergyPerLapWh={targetEnergyPerLapWh}
                  averages={lapAverages}
                />
              </Panel>
              <PackStatusPanel state={liveState} displayState={filteredLiveState} soeCutoffCellV={soeCutoffCellV} />
              {/* Live energy-plan tiles: used vs remaining + laps done/left +
                  live per-lap budget that recomputes from energy used each
                  completed lap. Configured up in Live Setup; surfaced here
                  next to Pack Status so the strategist reads pacing in one
                  glance with pack state. */}
              {dynamicLapBudgetWh != null ? (
                <Panel
                  title="Energy Plan"
                  icon={<Zap size={18} />}
                  headerRight={
                    <>Laps <strong>{lapsCompletedPlan}</strong> done · <strong>{lapsRemainingPlan ?? "—"}</strong> left</>
                  }
                >
                  <div className="energyPlanTiles">
                    <Metric label="Used / Remaining" value={`${energyUsedWh.toFixed(0)} / ${(remainingBudgetWh ?? 0).toFixed(0)} Wh`} />
                    <Metric
                      label="Budget / lap (live)"
                      value={`${dynamicLapBudgetWh.toFixed(0)} Wh`}
                      tone={budgetVsTargetWh != null && budgetVsTargetWh >= 0 ? "good" : ""}
                    />
                  </div>
                  {budgetVsTargetWh != null ? (
                    <small style={{ color: budgetVsTargetWh >= 0 ? "#5cb87a" : "#ff4d4f", fontWeight: 600, display: "block", marginTop: 6 }}>
                      {budgetVsTargetWh >= 0 ? "▲" : "▼"} {Math.abs(budgetVsTargetWh).toFixed(0)} Wh/lap {budgetVsTargetWh >= 0 ? "under" : "over"} budget — recalculates live each completed lap from energy used.
                    </small>
                  ) : null}
                </Panel>
              ) : null}
            </div>
            <div className="liveMainColumn">
              {/* Live Position hidden until chudpi/GPS is reliable. The RadioLion
                  receiver's draw browns out the chudpi rail, so no fix reaches
                  the website. Re-enable this Panel block once chudpi power is
                  fixed. */}
              {/*
              <Panel title="Live Position" icon={<MapPinned size={18} />} className="liveMapPanel">
                <TrackBuilderMap
                  points={filteredLiveState.samples.filter(hasGps).map((sample) => ({ t: sample.t, lat: sample.lat ?? 0, lon: sample.lon ?? 0 }))}
                  liveSample={filteredLiveState.lastSample}
                  gates={track.gates}
                  drawMode={null}
                  onDrawGate={handleDrawGate}
                  center={builderCenter}
                  onCenter={setBuilderCenter}
                  targetSpanM={selectedTrackView?.spanM}
                  gpsUnavailable={filteredLiveState.lastSample != null && !filteredLiveState.samples.some(hasGps)}
                />
              </Panel>
              */}
              <Panel title="Energy Window" icon={<Zap size={18} />}>
                <EnergyWindowChart
                  state={filteredLiveState}
                  windowS={energyWindowS}
                  onWindowS={setEnergyWindowS}
                />
              </Panel>
              <Panel title="Temperature Window" icon={<Thermometer size={18} />}>
                <TemperatureWindowChart
                  state={filteredLiveState}
                  windowS={energyWindowS}
                />
              </Panel>
              <Panel title="Vitals Window" icon={<Gauge size={18} />}>
                <VitalsWindowChart state={filteredLiveState} windowS={energyWindowS} />
              </Panel>
              {/* G-Forces hidden until the chudpi power issue is fixed — the
                  RadioLion GPS receiver's draw is browning out the chudpi rail
                  so no PIMU sentences reach cand. Re-enable with <GForcePanel
                  state={filteredLiveState} /> once the chudpi holds a stable
                  link. */}
              <DriverControlsPanel sample={filteredLiveState.lastSample} torqueParamSet={torqueParamSet} />
              <TempsStatusPanel sample={filteredLiveState.lastSample} />
            </div>
            <div className="rightRail">
              {/* Energy Strategy widget hidden for now — revisit later.
              <EnergyStrategyPanel
                state={liveState}
                targetLaps={targetLaps}
                targetEnergyKwh={targetEnergyKwh}
                targetEnergyPerLapWh={targetEnergyPerLapWh}
                soeCutoffCellV={soeCutoffCellV}
                averages={lapAverages}
              />
              */}
              <Panel title="Live Data" icon={<Gauge size={18} />}>
                <LiveDataPanel state={filteredLiveState} />
              </Panel>
              <Panel title="Live Setup" icon={<SlidersHorizontal size={18} />}>
                <div className="trackForm">
                  <label>
                    <span>Car</span>
                    <select
                      value={source}
                      disabled={liveState.running}
                      onChange={(e) => setSource(e.target.value as "orion" | "angelique")}
                    >
                      {sources.map((item) => <option key={item.key} value={item.key}>{item.label}</option>)}
                    </select>
                  </label>
                  <label>
                    <span>Track</span>
                    <select value={track.slug} onChange={(e) => setTrack(normalizeTrack(tracks.find((t) => t.slug === e.target.value) ?? track))}>
                      <option value={track.slug}>{track.name}</option>
                      {tracks.filter((t) => t.slug !== track.slug).map((t) => <option key={t.slug} value={t.slug}>{t.name}</option>)}
                    </select>
                  </label>
                  <label>
                    <span>Channel chart</span>
                    <select value={channelChart.slug} onChange={(e) => setChannelChart(channelCharts.find((chart) => chart.slug === e.target.value) ?? channelChart)}>
                      <option value={channelChart.slug}>{channelChart.name}</option>
                      {channelCharts.filter((chart) => chart.slug !== channelChart.slug).map((chart) => <option key={chart.slug} value={chart.slug}>{chart.name}</option>)}
                    </select>
                  </label>
                  {/* Connection knobs are dev/debug only — prod streams the car
                      automatically. Tucked away so the focused setup stays clean. */}
                  <details className="advancedConn">
                    <summary>Connection (advanced)</summary>
                    <label>
                      <span>Topic source</span>
                      <select value={liveTransport} disabled={liveState.running} onChange={(e) => setLiveTransport(e.target.value as KafkaTransport)}>
                        <option value="local">Local replay bus (simulator)</option>
                        <option value="kafka">Kafka broker (car)</option>
                        <option value="mqtt">MQTT broker (car direct)</option>
                      </select>
                    </label>
                    <label>
                      <span>Topic</span>
                      <input value={liveTopic} placeholder={liveTransport === "mqtt" ? "orion" : `grafana_data_${source}`} disabled={liveState.running} onChange={(e) => setLiveTopic(e.target.value)} />
                    </label>
                    <label>
                      <span>Sample rate</span>
                      <select value={liveSampleHz} disabled={liveState.running} onChange={(e) => setLiveSampleHz(Number(e.target.value))}>
                        <option value={1}>1 Hz</option>
                        <option value={2}>2 Hz</option>
                        <option value={5}>5 Hz</option>
                        <option value={10}>10 Hz</option>
                        <option value={20}>20 Hz</option>
                      </select>
                    </label>
                  </details>
                  <label>
                    <span>Race laps (synced)</span>
                    <input
                      type="number"
                      min={0}
                      value={targetLaps || ""}
                      placeholder="e.g. 22"
                      title="Total race laps — shared with every trackside client; drives the energy budget"
                      onChange={(e) => {
                        const n = Math.max(0, Math.floor(Number(e.target.value) || 0));
                        setTargetLaps(n);
                        racePlan.push(n, targetEnergyKwh, soeCutoffCellV);
                      }}
                    />
                  </label>
                  <label>
                    <span>Usable pack budget kWh (synced)</span>
                    <input
                      type="number"
                      min={0}
                      step={0.1}
                      value={targetEnergyKwh || ""}
                      placeholder={PACK_ENERGY_KWH.toFixed(2)}
                      onChange={(e) => {
                        const v = Math.max(0, Number(e.target.value) || 0);
                        setTargetEnergyKwh(v);
                        racePlan.push(targetLaps, v, soeCutoffCellV);
                      }}
                    />
                  </label>
                  <label>
                    <span>OCV cutoff V</span>
                    <input
                      type="number"
                      min={2}
                      max={4.2}
                      step={0.01}
                      value={soeCutoffCellV || ""}
                      placeholder={DEFAULT_SOE_CUTOFF_CELL_V.toFixed(2)}
                      disabled={isMirror}
                      onChange={(e) => {
                        const v = normalizeSoeCutoffCellV(Number(e.target.value) || DEFAULT_SOE_CUTOFF_CELL_V);
                        setSoeCutoffCellV(v);
                        racePlan.push(targetLaps, targetEnergyKwh, v);
                      }}
                    />
                  </label>
                  {targetEnergyPerLapWh != null ? (
                    <small className="muted">
                      Usable budget {targetEnergyKwh.toFixed(2)} kWh to {soeCutoffCellV.toFixed(2)} V min-cell OCV ÷ {targetLaps} = {targetEnergyPerLapWh.toFixed(0)} Wh/lap even split.
                    </small>
                  ) : null}
                  {/* Live energy-plan tiles moved to the Energy Plan panel
                      under Pack Status — they read better next to pack state
                      than buried under the configuration inputs. */}
                  <div className="feedControl">
                    <div>
                      <strong>Live feed</strong>
                      <small>Auto-starts on load and reconnects if it drops — no need to start it.</small>
                    </div>
                    <span className="freshTag"><span className={`dot ${uplinkMeta.dot}`} /> {uplinkMeta.label}</span>
                  </div>
                  {sessionInfo ? (
                    <div className="sessionTag" title={`Started ${new Date(sessionInfo.startedAt).toLocaleString()}`}>
                      <strong>{sessionInfo.name}</strong>
                      <small>{[sessionInfo.eventType, sessionInfo.driver, sessionInfo.venue].filter(Boolean).join(" · ") || "no metadata"}</small>
                    </div>
                  ) : null}
                  <div className="sessionFileRow">
                    <button type="button" className="primary" disabled={isMirror} onClick={() => setNewSessionOpen(true)}>
                      <Plus size={15} /> New Session
                    </button>
                    <button type="button" className="tool" onClick={() => setSetupOpen(true)} title="Drive-day setup: conditions, alignment, shocks, tires, aero — saved to the session's drive_day record">
                      <SlidersHorizontal size={15} /> Drive Day Setup
                    </button>
                    <button type="button" className="tool" onClick={saveSessionFile}>
                      <Save size={15} /> Save JSON
                    </button>
                    <button type="button" className="tool" disabled={!liveState.laps.length} onClick={downloadSessionCsv}>
                      <Download size={15} /> Laps CSV
                    </button>
                    <button type="button" className="tool" onClick={() => sessionFileInputRef.current?.click()}>
                      <Upload size={15} /> Load
                    </button>
                    <input
                      ref={sessionFileInputRef}
                      type="file"
                      accept="application/json,.json"
                      style={{ display: "none" }}
                      onChange={loadSessionFile}
                    />
                  </div>
                  <div className="dangerZone">
                    <div>
                      <strong>Reset saved run</strong>
                      <small>Clears laps + cache and keeps streaming. Use New Session to also tag a fresh run.</small>
                    </div>
                    <button type="button" className="tool dangerTool" disabled={isMirror} onClick={resetLiveSession}>
                      <Trash2 size={15} /> Reset All
                    </button>
                  </div>
                </div>
              </Panel>
              <Panel title="Pre-Set Metadata" icon={<FileText size={18} />}>
                <MetadataFields
                  compact
                  values={metadataDraft}
                  onChange={(key, value) => setMetadataDraft((prev) => ({ ...prev, [key]: value }))}
                />
              </Panel>
            </div>
          </div>
        </section>
      ) : null}

      {activeTab === "track-builder" ? (
        <section className="builderShell">
          <aside className="builderSidebar">
            <Panel title="Search Data" icon={<CalendarDays size={18} />}>
              <div className="sourceRow">
                <select value={source} onChange={(e) => setSource(e.target.value as "orion" | "angelique")}>
                  {sources.map((item) => <option key={item.key} value={item.key}>{item.label}</option>)}
                </select>
                <button className="tool iconOnly" onClick={() => refreshCalendar()} aria-label="Refresh"><RefreshCcw size={16} /></button>
              </div>
              <div className="builderControls">
                <select value={channel} onChange={(e) => setChannel(e.target.value)}>
                  {sortedChannels.map((c) => <option key={c.key} value={c.key}>{c.label}</option>)}
                </select>
                <input type="number" value={threshold} onChange={(e) => setThreshold(Number(e.target.value))} />
              </div>
              <div className="dayList builderDayList">
                {days.map((day) => (
                  <button key={day.date} className={day.date === selectedDate ? "day selected" : "day"} onClick={() => setSelectedDate(day.date)}>
                    <strong>{day.date}</strong>
                    <span>{sessionCountsByDate[day.date] ?? day.sessions} sessions</span>
                  </button>
                ))}
              </div>
            </Panel>
            <Panel title="Reference Sessions" icon={<Scissors size={18} />}>
              <div className="stack compact builderSessionList">
                {segments.map((segment) => (
                  <div key={segment.id} className={previewSelectedSegments.has(segment.id) ? "sessionRow previewSelected" : "sessionRow"} role="button" tabIndex={0} onClick={(event) => handleSessionClick(event, segment)}>
                    <input type="checkbox" checked={previewSelectedSegments.has(segment.id)} readOnly />
                    <span>
                      <span className="sessionTitle">
                        <strong>{segment.label}</strong>
                        <SessionGpsBadge segment={segment} />
                      </span>
                      <small>{formatTime(segment.start_ms)} - {formatDuration(segment.duration_s)}</small>
                    </span>
                  </div>
                ))}
                {!segments.length ? <small className="muted">Load a day to use historic GPS as a drawing reference.</small> : null}
              </div>
            </Panel>
          </aside>

          <Panel title="Track Map" icon={<MapPinned size={18} />} className="builderMapPanel">
            <div className="mapToolbar">
              <input value={builderSearch} onChange={(e) => setBuilderSearch(e.target.value)} placeholder="lat, lon" />
              <button className="tool" onClick={() => {
                const parsed = parseLatLon(builderSearch);
                if (parsed) setBuilderCenter(parsed);
              }}><Target size={15} /> Center</button>
              <button className="tool" onClick={() => {
                navigator.geolocation?.getCurrentPosition((position) => {
                  setBuilderCenter({ lat: position.coords.latitude, lon: position.coords.longitude });
                  setBuilderSearch(`${position.coords.latitude.toFixed(6)}, ${position.coords.longitude.toFixed(6)}`);
                }, (geoError) => setError(geoError.message));
              }}><MapPinned size={15} /> My Location</button>
            </div>
            <TrackBuilderMap
              points={gps}
              liveSample={null}
              gates={track.gates}
              drawMode={gateDrawMode}
              onDrawGate={handleDrawGate}
              center={builderCenter}
              onCenter={setBuilderCenter}
              targetSpanM={selectedTrackView?.spanM}
            />
          </Panel>

          <div className="rightRail">
            <TrackManagerPanel
              track={track}
              tracks={tracks}
              hasStartFinish={hasStartFinish}
              gateDrawMode={gateDrawMode}
              onSetTrack={setTrack}
              onNewTrack={newTrack}
              onSetGateDrawMode={setGateDrawMode}
              onSaveTrack={saveTrack}
              onUploadTrack={uploadTrack}
              onDownloadTrack={downloadTrack}
              onUpdateGate={updateGate}
              onMoveSplitGate={moveSplitGate}
              onRemoveGate={removeGate}
            />
          </div>
        </section>
      ) : null}

      {activeTab === "race-ops" ? (
        <section className="opsGrid">
          <Panel title="Run Plan" icon={<NotebookText size={18} />}>
            <div className="opsCards">
              <Metric label="Car" value={sourceLabel} />
              <Metric label="Track" value={track.name} />
              <Metric label="Channel Chart" value={channelChart.name} />
              <Metric label="Selected Sessions" value={`${selectedSegments.size}`} />
            </div>
            <div className="opsChecklist">
              {[
                { label: "Start / finish gate set", done: hasStartFinish },
                { label: "Split gates added", done: splitGates.length > 0 },
                { label: "Metadata prefilled (driver & event)", done: !!metadataDraft.driver.trim() && !!metadataDraft.event.trim() },
                { label: "Channel chart has entries", done: channelChart.entries.length > 0 },
                { label: "Energy target configured", done: targetLaps > 0 && targetEnergyKwh > 0 },
              ].map((item) => (
                <label key={item.label} className={item.done ? "checkInline done" : "checkInline"}>
                  <input type="checkbox" checked={item.done} readOnly /> {item.label}
                </label>
              ))}
            </div>
          </Panel>
          <Panel title="Energy Brief" icon={<Zap size={18} />}>
            <div className="opsCards">
              <Metric label="Inv Used" value={`${liveState.totalEnergyWh.toFixed(1)} Wh`} />
              <Metric label="Last Lap" value={liveState.laps.at(-1) ? `${liveState.laps.at(-1)!.energyWh.toFixed(1)} Wh` : "--"} />
              <Metric label="Best Lap" value={bestLap ? formatLapTime(bestLap.durationMs) : "--"} tone="purple" />
            </div>
          </Panel>
          <Panel title="Car Presets" icon={<SlidersHorizontal size={18} />} className="carPresetPanel">
            <div className="presetToolbar">
              <select
                value={selectedCarPresetId}
                onChange={(e) => {
                  const preset = carPresets.find((item) => item.id === e.target.value);
                  if (!preset) return;
                  setSelectedCarPresetId(preset.id);
                  setCarPresetDraft(preset);
                }}
              >
                {carPresets.map((preset) => <option key={preset.id} value={preset.id}>{preset.name}</option>)}
              </select>
              <button className="tool" onClick={() => applyCarPreset()}><Target size={15} /> Apply</button>
              <button className="tool" onClick={newCarPreset}><Plus size={15} /> New</button>
              <button className="tool dangerTool" onClick={deleteCarPreset}><Trash2 size={15} /> Delete</button>
            </div>
            <div className="presetGrid">
              <label>
                <span>Name</span>
                <input value={carPresetDraft.name} onChange={(e) => setCarPresetDraft((prev) => ({ ...prev, name: e.target.value }))} />
              </label>
              <label>
                <span>Source</span>
                <select value={carPresetDraft.source} onChange={(e) => setCarPresetDraft((prev) => ({ ...prev, source: e.target.value as "orion" | "angelique" }))}>
                  {sources.map((item) => <option key={item.key} value={item.key}>{item.label}</option>)}
                </select>
              </label>
              <label>
                <span>Topic</span>
                <input value={carPresetDraft.topic} onChange={(e) => setCarPresetDraft((prev) => ({ ...prev, topic: e.target.value }))} />
              </label>
              <label>
                <span>Topic source</span>
                <select value={carPresetDraft.transport} onChange={(e) => setCarPresetDraft((prev) => ({ ...prev, transport: e.target.value as KafkaTransport }))}>
                  <option value="local">Local replay bus</option>
                  <option value="kafka">Kafka broker</option>
                </select>
              </label>
              <label>
                <span>Track</span>
                <select value={carPresetDraft.trackSlug} onChange={(e) => setCarPresetDraft((prev) => ({ ...prev, trackSlug: e.target.value }))}>
                  <option value={track.slug}>{track.name}</option>
                  {tracks.filter((item) => item.slug !== track.slug).map((item) => <option key={item.slug} value={item.slug}>{item.name}</option>)}
                </select>
              </label>
              <label>
                <span>Channel chart</span>
                <select value={carPresetDraft.channelChartSlug} onChange={(e) => setCarPresetDraft((prev) => ({ ...prev, channelChartSlug: e.target.value }))}>
                  <option value={channelChart.slug}>{channelChart.name}</option>
                  {channelCharts.filter((item) => item.slug !== channelChart.slug).map((item) => <option key={item.slug} value={item.slug}>{item.name}</option>)}
                </select>
              </label>
            </div>
            <div className="presetMetadata">
              <MetadataFields
                compact
                values={carPresetDraft.metadata}
                onChange={(key, value) => setCarPresetDraft((prev) => ({ ...prev, metadata: { ...prev.metadata, [key]: value } }))}
              />
            </div>
            <div className="gateButtons">
              <button className="primary" onClick={saveCarPreset}><Save size={15} /> Save Preset</button>
            </div>
          </Panel>
        </section>
      ) : null}

      {activeTab === "dash" ? (
        <section className="opsGrid">
          <Panel title="Dash Link" icon={<Radio size={18} />}>
            <div className="opsCards">
              <Metric
                label="Status"
                value={
                  dashSignals.status === "connected" ? "Connected"
                  : dashSignals.status === "connecting" ? "Connecting…"
                  : dashSignals.status === "error" ? "Error"
                  : dashSignals.status === "closed" ? "Disconnected"
                  : "Idle"
                }
                tone={dashSignals.status === "connected" ? "good" : ""}
              />
              <Metric label="Laps Sent" value={String(dashSignals.lapsSent)} />
            </div>
            <p style={{ margin: "4px 0 10px", opacity: 0.7, fontSize: "0.85rem" }}>
              Links to the car dash over the secure broker — one source, no setup.
            </p>
            <div className="gateButtons">
              {dashSignals.status === "connected" || dashSignals.status === "connecting" ? (
                <button className="tool dangerTool" onClick={dashSignals.disconnect}><X size={15} /> Disconnect</button>
              ) : (
                <button className="primary" disabled={isMirror} onClick={dashSignals.connect}><Radio size={15} /> Connect to dash</button>
              )}
            </div>
            {dashSignals.error ? (
              <div className="error" role="alert"><span>{dashSignals.error}</span></div>
            ) : null}
          </Panel>

          <Panel title="Power Budget" icon={<Zap size={18} />}>
            <div className="themeToggle" role="group" aria-label="Power budget mode" style={{ marginBottom: 10 }}>
              <button className={dashPowerMode === "auto" ? "on" : ""} disabled={isMirror} onClick={() => setDashPowerMode("auto")}>Auto (from laps)</button>
              <button className={dashPowerMode === "manual" ? "on" : ""} disabled={isMirror} onClick={() => setDashPowerMode("manual")}>Manual</button>
            </div>
            <div style={{ display: "flex", alignItems: "baseline", gap: 10, marginBottom: 8 }}>
              <strong style={{ fontSize: "3rem", lineHeight: 1 }}>{effectiveTargetPowerKw != null ? effectiveTargetPowerKw.toFixed(0) : "--"}</strong>
              <span>kW target {dashPowerMode === "auto" ? "(auto)" : "(manual)"}</span>
            </div>
            {dashPowerMode === "auto" ? (
              <p style={{ margin: "0 0 8px", opacity: 0.75, fontSize: "0.82rem" }}>
                {dynamicLapBudgetWh != null && avgLapTimeS != null
                  ? `${dynamicLapBudgetWh.toFixed(0)} Wh/lap ÷ ${avgLapTimeS.toFixed(0)}s avg lap = ${effectiveTargetPowerKw != null ? effectiveTargetPowerKw.toFixed(1) : "--"} kW.`
                  : "Set race laps + energy budget (Live tab) and complete a lap so the average lap time is known."}
              </p>
            ) : (
              <>
                <input
                  type="range"
                  min={0}
                  max={80}
                  step={1}
                  value={dashTargetPower}
                  disabled={isMirror}
                  onChange={(e) => setDashTargetPower(Number(e.target.value))}
                  style={{ width: "100%" }}
                />
                <div className="gateButtons" style={{ marginTop: 8 }}>
                  <button className="tool" disabled={isMirror} onClick={() => setDashTargetPower((p) => Math.max(0, Math.round(p) - 1))}><ArrowDown size={15} /> -1</button>
                  <button className="tool" disabled={isMirror} onClick={() => setDashTargetPower((p) => Math.min(120, Math.round(p) + 1))}><ArrowUp size={15} /> +1</button>
                  <input
                    type="number"
                    value={dashTargetPower}
                    min={0}
                    max={120}
                    disabled={isMirror}
                    onChange={(e) => setDashTargetPower(Number(e.target.value))}
                    style={{ width: 90 }}
                  />
                </div>
              </>
            )}
            <p style={{ marginTop: 10, opacity: 0.7, fontSize: "0.85rem" }}>
              Dash energy bar runs green while the car draws under this budget, red when over.
              {dashPowerMode === "auto"
                ? " Auto-derived from the energy plan (Wh/lap ÷ avg lap time) — switch to Manual to force a kW."
                : " Manual override — switch to Auto to track the energy plan."}
              {" "}Sent live + republished ~1 Hz so it never goes stale.
            </p>
          </Panel>

          <Panel title="Lap Control" icon={<Flag size={18} />}>
            <button
              className="primary"
              style={{ fontSize: "1.3rem", padding: "16px 18px", width: "100%", justifyContent: "center" }}
              disabled={isMirror || !telemetryFresh}
              title={isMirror ? "Read-only mirror" : !telemetryFresh ? "No live telemetry from the car" : "Same action as the Live tab's Start/Log Lap button"}
              onClick={markLap}
            >
              <Flag size={18} /> {liveState.lapStartMs ? "Log Lap" : "Start Lap"}
            </button>
            <p style={{ margin: "8px 0 0", opacity: 0.7, fontSize: "0.82rem" }}>
              The same button as the Live tab&apos;s <strong>{liveState.lapStartMs ? "Log Lap" : "Start Lap"}</strong>:
              it logs the lap on the Live tab and fires the driver&apos;s dash lap card on the car
              {dashSignals.status === "connected" ? "." : " (uplink connecting…)."}
            </p>
            <label className="checkInline" style={{ marginTop: 10 }}>
              <input type="checkbox" checked={dashAutoLap} onChange={(e) => setDashAutoLap(e.target.checked)} />
              Auto-send on completed lap (uses the live lap detector)
            </label>
            <div className="opsCards" style={{ marginTop: 10 }}>
              <Metric label="Last Lap" value={liveState.laps.at(-1) ? formatLapTime(liveState.laps.at(-1)!.durationMs) : "--"} />
              <Metric label="Last Lap NRG" value={liveState.laps.at(-1) ? `${liveState.laps.at(-1)!.energyWh.toFixed(0)} Wh` : "--"} />
              <Metric label="Best Lap" value={bestLap ? formatLapTime(bestLap.durationMs) : "--"} tone="purple" />
            </div>
            <div className="gateButtons" style={{ marginTop: 10 }}>
              <button className="tool" disabled={isMirror} onClick={() => setLapDesignerOpen(true)}>
                <SlidersHorizontal size={15} /> Design lap screen
              </button>
            </div>
            <label style={{ marginTop: 10 }}>
              <span>Lap screen duration (s)</span>
              <input
                type="number"
                min={1}
                max={30}
                step={0.5}
                value={lapCardDurationS || ""}
                disabled={isMirror}
                title="How long the full-screen lap card stays up on the driver's dash after each lap. Sent to the car (retained)."
                onChange={(e) => {
                  const v = Math.max(1, Math.min(30, Number(e.target.value) || 5));
                  setLapCardDurationS(v);
                }}
              />
            </label>
            <p style={{ margin: "6px 0 0", opacity: 0.7, fontSize: "0.8rem" }}>
              Lay out what the driver sees on each lap card, then send it to the car (used until replaced).
              The lap card auto-clears after the duration above.
            </p>
          </Panel>

          <Panel title="Start/Finish — on-car laps" icon={<MapPinned size={18} />}>
            {(() => {
              const sfGate = track.gates.find((gate) => gate.role === "start_finish");
              return (
                <>
                  <div className="opsCards">
                    <Metric label="Track" value={track.name} />
                    <Metric label="S/F gate" value={sfGate ? "defined" : "none"} tone={sfGate ? "good" : ""} />
                  </div>
                  <p style={{ marginTop: 8, opacity: 0.7, fontSize: "0.85rem" }}>
                    Push the start/finish line so the car counts laps from its own GPS — the
                    per-lap reset then survives any cellular dropout. Send Lap stays as a manual
                    override. Define or edit the gate in Track Builder.
                  </p>
                  <div className="gateButtons" style={{ marginTop: 8 }}>
                    <button
                      className="primary"
                      disabled={isMirror || !sfGate || dashSignals.status !== "connected"}
                      onClick={() => {
                        if (!sfGate) return;
                        dashSignals.publishGate([sfGate.lat1, sfGate.lon1, sfGate.lat2, sfGate.lon2]);
                        setDashGatePushed(true);
                        window.setTimeout(() => setDashGatePushed(false), 2500);
                      }}
                    >
                      <Upload size={15} /> Push S/F to car
                    </button>
                    {dashGatePushed ? <span className="goodText">Pushed ✓ (retained)</span> : null}
                  </div>
                  {!sfGate ? (
                    <p style={{ marginTop: 6, opacity: 0.7, fontSize: "0.8rem" }}>
                      No start/finish gate on the loaded track yet — add one in the Track Builder tab.
                    </p>
                  ) : null}
                </>
              );
            })()}
          </Panel>

          <Panel title="Link Health & Dash Mirror" icon={<Activity size={18} />} className="dashMirrorPanel">
            {(() => {
              const ds = dashSignals.dashState;
              const at = dashSignals.lastStateAt;
              const acks = dashSignals.acks;
              const ago = (t: number) => `${Math.max(0, Math.round((dashNow - t) / 1000))}s ago`;
              const linkLive = at !== null && dashNow - at < 3000;
              const fmt = (v: number | null | undefined, d = 0, unit = "") =>
                v === null || v === undefined ? "--" : `${v.toFixed(d)}${unit}`;
              const delta = ds?.pacing.budgetDeltaWh ?? null;
              return (
                <>
                  <div className="opsCards">
                    <Metric label="Dash link" value={at === null ? "no data" : linkLive ? "LIVE" : `SILENT ${ago(at)}`} tone={linkLive ? "good" : ""} />
                    <Metric label="Budget heard" value={acks.targetPower ? `${acks.targetPower.value.toFixed(0)} kW · ${ago(acks.targetPower.at)}` : "--"} tone={acks.targetPower ? "good" : ""} />
                    <Metric label="Gate acked" value={acks.sfGate ? ago(acks.sfGate.at) : "no"} tone={acks.sfGate ? "good" : ""} />
                  </div>
                  <div className="opsCards" style={{ marginTop: 8 }}>
                    <Metric label="Speed" value={fmt(ds?.speed, 0, " mph")} />
                    <Metric label="Power" value={fmt(ds?.power, 1, " kW")} />
                    <Metric label="SOC" value={fmt(ds?.soc, 0, " %")} />
                  </div>
                  <div className="opsCards" style={{ marginTop: 8 }}>
                    <Metric label={`Lap ${ds?.pacing.lapNumber ?? "--"} NRG`} value={fmt(ds?.pacing.lapEnergyWh, 0, " Wh")} />
                    <Metric label="Budget Δ" value={delta === null ? "--" : `${delta > 0 ? "+" : ""}${delta.toFixed(0)} Wh`} tone={delta !== null && delta < 0 ? "good" : ""} />
                    <Metric label="Car laps" value={ds ? String(ds.lapCount) : "--"} />
                  </div>
                  {ds?.targetPowerStale ? (
                    <p style={{ marginTop: 6, color: "#c47", fontSize: "0.8rem" }}>
                      Driver&apos;s budget is showing STALE — the car hasn&apos;t heard a fresh target in 5s.
                    </p>
                  ) : null}
                  {!linkLive && at !== null ? (
                    <p style={{ marginTop: 6, color: "#c44", fontSize: "0.8rem" }}>
                      Dash state silent — uplink may be down. The driver still has the held budget and on-car laps.
                    </p>
                  ) : null}
                </>
              );
            })()}
          </Panel>

          <Panel title="Driver Messages" icon={<NotebookText size={18} />}>
            {(() => {
              const active = activeMessages(msgLib.lib);
              const linkReady = !isMirror && dashSignals.status === "connected";
              const gateTitle = isMirror ? "Read-only mirror" : dashSignals.status !== "connected" ? "Connect to the dash first" : undefined;
              return (
                <>
                  <p style={{ margin: "0 0 10px", opacity: 0.7, fontSize: "0.82rem" }}>
                    Tap to flash a message on the driver&apos;s dash. Edit the palette to add your own
                    (color, icon, and how long each stays on screen).
                  </p>
                  {active.length ? (
                    <div className="msgQuickRow">
                      {active.map((m) => (
                        <button
                          key={m.id}
                          className="msgQuickBtn"
                          disabled={!linkReady}
                          title={gateTitle ?? (m.durationS ? `${m.text} — ${m.durationS}s` : `${m.text} — until cleared`)}
                          onClick={() => sendDriverMessage(m)}
                        >
                          {MESSAGE_ICON_GLYPH[m.icon] ? <span className="msgQuickGlyph">{MESSAGE_ICON_GLYPH[m.icon]}</span> : null}
                          <span className="msgQuickLabel">{m.label}</span>
                        </button>
                      ))}
                    </div>
                  ) : (
                    <p style={{ opacity: 0.7, fontSize: "0.82rem" }}>
                      No active messages — open the editor and star a few to add quick-send buttons.
                    </p>
                  )}
                  <div className="gateButtons" style={{ marginTop: 12 }}>
                    <button className="tool dangerTool" disabled={!linkReady} title={gateTitle} onClick={clearDriverMessage}>
                      <X size={15} /> Clear driver&apos;s screen
                    </button>
                    <button className="tool" disabled={isMirror} onClick={() => setMsgEditorOpen(true)}>
                      <SlidersHorizontal size={15} /> Edit messages
                    </button>
                  </div>
                  {msgSendStatus ? <p className="goodText" style={{ marginTop: 8, fontSize: "0.84rem" }}>{msgSendStatus}</p> : null}
                </>
              );
            })()}
          </Panel>
        </section>
      ) : null}
    </main>
  );
}

function Panel({ title, icon, children, className = "", headerRight }: { title: string; icon: ReactNode; children: ReactNode; className?: string; headerRight?: ReactNode }) {
  return (
    <section className={className ? `panel ${className}` : "panel"}>
      <div className="panelTitle">
        {icon}<h2>{title}</h2>
        {headerRight ? <span className="panelTitleRight">{headerRight}</span> : null}
      </div>
      {children}
    </section>
  );
}

const METADATA_LONG_FIELDS = new Set<MetadataField>(["vehicle_comment", "short_comment", "long_comment"]);

// Single source of truth for the MoTeC metadata field grid — used by the
// exporter, the live pre-set panel, and the car-preset editor (previously three
// near-identical copies).
function NewSessionModal({
  sources,
  defaultCar,
  defaultVenue,
  defaultDriver,
  onStart,
  onClose,
}: {
  sources: SourceDef[];
  defaultCar: "orion" | "angelique";
  defaultVenue: string;
  defaultDriver: string;
  onStart: (draft: { name: string; car: "orion" | "angelique"; driver: string; eventType: string; venue: string }) => void;
  onClose: () => void;
}) {
  const [car, setCar] = useState<"orion" | "angelique">(defaultCar);
  const [driver, setDriver] = useState(defaultDriver);
  const [eventType, setEventType] = useState<string>(EVENT_TYPES[0]);
  const [venue, setVenue] = useState(defaultVenue);
  const [name, setName] = useState("");
  const suggestedName = `${venue || "Session"} · ${eventType}`;

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === "Escape") onClose(); };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onClose]);

  return (
    <div className="modalOverlay" onMouseDown={onClose}>
      <div className="modalCard" onMouseDown={(e) => e.stopPropagation()}>
        <div className="modalHead">
          <h3>New session</h3>
          <button className="tool iconOnly" onClick={onClose} aria-label="Close"><X size={16} /></button>
        </div>
        <div className="trackForm">
          <label>
            <span>Session name</span>
            <input value={name} placeholder={suggestedName} autoFocus onChange={(e) => setName(e.target.value)} />
          </label>
          <label>
            <span>Car</span>
            <select value={car} onChange={(e) => setCar(e.target.value as "orion" | "angelique")}>
              {sources.map((s) => <option key={s.key} value={s.key}>{s.label}</option>)}
            </select>
          </label>
          <label>
            <span>Event type</span>
            <select value={eventType} onChange={(e) => setEventType(e.target.value)}>
              {EVENT_TYPES.map((ev) => <option key={ev} value={ev}>{ev}</option>)}
            </select>
          </label>
          <label>
            <span>Driver</span>
            <input value={driver} placeholder="Name" onChange={(e) => setDriver(e.target.value)} />
          </label>
          <label>
            <span>Venue / track</span>
            <input value={venue} onChange={(e) => setVenue(e.target.value)} />
          </label>
          <small className="muted">
            Starts now and clears the current run. The name, driver, event and venue tag this
            session so its log CSV auto-fills in Log Sync.
          </small>
        </div>
        <div className="modalFoot">
          <button type="button" className="tool" onClick={onClose}>Cancel</button>
          <button
            type="button"
            className="primary"
            onClick={() => onStart({ name: name.trim() || suggestedName, car, driver, eventType, venue })}
          >
            <Flag size={15} /> Start session
          </button>
        </div>
      </div>
    </div>
  );
}

function MetadataFields({
  values,
  onChange,
  mixed,
  compact = false,
}: {
  values: SessionMetadata;
  onChange: (key: MetadataField, value: string) => void;
  mixed?: Set<MetadataField>;
  compact?: boolean;
}) {
  return (
    <div className={compact ? "metadataGrid compactMetadata" : "metadataGrid"}>
      {METADATA_FIELDS.map((key) => {
        const isMixed = mixed?.has(key) ?? false;
        const isLong = METADATA_LONG_FIELDS.has(key);
        return (
          <label key={key} className={isLong ? "span2" : ""}>
            <span>{key.replace(/_/g, " ")}</span>
            {key === "long_comment" ? (
              <textarea
                value={values[key]}
                placeholder={isMixed ? "Mixed values" : ""}
                className={isMixed ? "mixedField" : ""}
                onChange={(e) => onChange(key, e.target.value)}
              />
            ) : (
              <input
                value={values[key]}
                placeholder={isMixed ? "Mixed values" : ""}
                className={isMixed ? "mixedField" : ""}
                onChange={(e) => onChange(key, e.target.value)}
              />
            )}
          </label>
        );
      })}
    </div>
  );
}

function formatStateDuration(ms: number): string {
  const totalS = Math.max(0, Math.round(ms / 1000));
  if (totalS < 60) return `${totalS}s`;
  const m = Math.floor(totalS / 60);
  const s = totalS % 60;
  if (m < 60) return `${m}m ${s.toString().padStart(2, "0")}s`;
  const h = Math.floor(m / 60);
  return `${h}h ${(m % 60).toString().padStart(2, "0")}m`;
}

function CarStatusPanel({
  feed,
  carState,
  stale,
  ageMs,
  uplinkLabel,
  uplinkDot,
  running,
}: {
  feed: CarStatusFeed;
  carState: CarState | null;
  stale: boolean;
  ageMs: number | null;
  uplinkLabel: string;
  uplinkDot: string;
  running: boolean;
}) {
  const ev = feed.event;
  return (
    <Panel title="Car Status" icon={<Power size={18} />}>
      <div className="carStatusCard">
        <div className="carStatusHead">
          {carState ? (
            <span className={`carStateBadge ${CAR_STATE_META[carState].cls}`}>
              <span className="stateDot" /> {CAR_STATE_META[carState].label}
            </span>
          ) : (
            <span className="carStateBadge stateUnknown">
              <span className="stateDot" /> {running ? "Unknown" : "Standby"}
            </span>
          )}
          <span className="freshTag"><span className={`dot ${uplinkDot}`} /> {uplinkLabel}</span>
        </div>
        {feed.available ? (
          <>
            {ev?.reasons?.length ? (
              <div className="carStatusReasons">{ev.reasons.map(humanizeReason).join(" · ")}</div>
            ) : null}
            {feed.faults.length ? (
              <div className="faultRow">
                {feed.faults.map((fault) => (
                  <span key={fault} className="faultChip"><AlertTriangle size={12} /> {humanizeReason(fault)}</span>
                ))}
              </div>
            ) : null}
            <div className="carStatusMetaGrid">
              <Metric label="HV SoC" value={ev?.hv_soc != null ? `${ev.hv_soc.toFixed(0)}%` : "--"} />
              <Metric label="HV Pack" value={ev?.hv_pack_v != null ? `${ev.hv_pack_v.toFixed(1)} V` : "--"} />
              <Metric label="LV Batt" value={ev?.lv_v != null ? `${ev.lv_v.toFixed(1)} V` : "--"} />
              <Metric label="In State" value={ev?.time_in_state_ms != null ? formatStateDuration(ev.time_in_state_ms) : "--"} />
            </div>
            {stale ? (
              <small className="muted">Classifier silent for {ageMs != null ? `${Math.round(ageMs / 1000)}s` : "a while"} — state may be stale.</small>
            ) : null}
          </>
        ) : (
          <div className="carStatusUnavailable">
            <WifiOff size={15} />
            <span>{running ? "Classifier feed unavailable — showing telemetry-flow status only." : "Start the live feed to read car state."}</span>
          </div>
        )}
      </div>
    </Panel>
  );
}

function Metric({ label, value, tone = "" }: { label: string; value: string; tone?: "purple" | "good" | "" }) {
  return (
    <div className="metric">
      <span>{label}</span>
      <strong className={tone === "purple" ? "purpleText" : tone === "good" ? "goodText" : ""}>{value}</strong>
    </div>
  );
}

function DeltaBar({ rate, totalMs }: { rate: number | null; totalMs: number }) {
  const bounded = Math.max(-1.5, Math.min(1.5, rate ?? 0));
  const width = Math.min(50, Math.abs(bounded) / 1.5 * 50);
  const gaining = bounded < 0;
  const style = gaining
    ? { left: `${50 - width}%`, width: `${width}%` }
    : { left: "50%", width: `${width}%` };
  return (
    <div className="deltaBox">
      <div className="deltaReadout">
        <strong>{rate == null ? "--" : `${rate >= 0 ? "+" : ""}${rate.toFixed(3)} s/s`}</strong>
        <span>{formatSignedSeconds(totalMs)}</span>
      </div>
      <div className="deltaTrack">
        <span className="deltaZero" />
        <span className={gaining ? "deltaFill gaining" : "deltaFill losing"} style={style} />
      </div>
    </div>
  );
}

function PackStatusPanel({
  state,
  displayState,
  soeCutoffCellV,
}: {
  state: LiveSessionState;
  displayState: LiveSessionState;
  soeCutoffCellV: number;
}) {
  const pack = packStatus(state.samples, soeCutoffCellV, displayState.lastSample);
  const sample = displayState.lastSample;
  const signedPowerKw = signedVcuPowerKwFor(displayState.previousSample, sample);
  const powerKw = signedPowerKw == null ? null : Math.abs(signedPowerKw);
  return (
    <Panel title="Pack Status" icon={<Zap size={18} />}>
      <div className="packStatus">
        <div className="socHeader">
          <span>SOE Est</span>
          <strong>
            {pack.soePercent == null ? "--" : `${pack.soePercent.toFixed(0)}%`}
            {pack.soeKwh == null ? null : <span className="socKwh">{pack.soeKwh.toFixed(2)} kWh</span>}
          </strong>
        </div>
        <div className="socBar" aria-label="Pack state of energy">
          <span style={{ width: `${pack.soePercent ?? 0}%` }} />
        </div>
        <small className="muted">
          {pack.soeSource === "car"
            ? `Using car SOE estimate${pack.minCellV != null ? `; live min cell ${pack.minCellV.toFixed(3)} V` : ""}.`
            : `Waiting for the car's reported SOC (VCU soc_estimate / hv_soc). The min-cell OCV fallback is disabled by request.`}
        </small>
        <div className="packMetricGrid">
          <Metric label={pack.voltageSource === "cell-est" ? "Pack V Est" : "Voltage"} value={pack.voltage == null ? "--" : `${pack.voltage.toFixed(1)} V`} />
          <Metric label="Power" value={powerKw == null ? "--" : `${powerKw.toFixed(2)} kW`} />
          <Metric label="OCV Derate" value={pack.lowVoltageDeratePct == null ? "--" : `${pack.lowVoltageDeratePct.toFixed(0)}%`} />
          <Metric label="Min Cell" value={pack.minCellV == null ? "--" : `${pack.minCellV.toFixed(3)} V`} />
          <Metric label="Max Cell" value={pack.maxCellV == null ? "--" : `${pack.maxCellV.toFixed(3)} V`} />
          <Metric label="Max Cell T" value={pack.maxCellTemp == null ? "--" : `${pack.maxCellTemp.toFixed(1)} C`} />
        </div>
      </div>
    </Panel>
  );
}

function EnergyStrategyPanel({
  state,
  targetLaps,
  targetEnergyKwh,
  targetEnergyPerLapWh,
  soeCutoffCellV,
  averages,
}: {
  state: LiveSessionState;
  targetLaps: number;
  targetEnergyKwh: number;
  targetEnergyPerLapWh: number | null;
  soeCutoffCellV: number;
  averages: {
    count: number;
    avgMs: number | null;
    avgEnergyWh: number | null;
    avgEnergyOutWh: number | null;
    avgEnergyInWh: number | null;
  };
}) {
  const sample = state.lastSample;
  const signedPowerKw = signedVcuPowerKwFor(state.previousSample, sample);
  const drivePowerKw = signedPowerKw == null ? null : Math.max(0, signedPowerKw);
  const regenPowerKw = signedPowerKw == null ? null : Math.max(0, -signedPowerKw);
  const pack = packStatus(state.samples, soeCutoffCellV, sample);
  const targetWh = targetEnergyKwh > 0 ? targetEnergyKwh * 1000 : null;
  const vcuUsedWh = state.totalEnergyWh;
  const soeUsedFromFullWh = pack.soePercent == null ? null : (1 - pack.soePercent / 100) * PACK_ENERGY_KWH * 1000;
  const soeRemainingWh = pack.soeKwh == null ? null : pack.soeKwh * 1000;
  const packUsedForBudgetWh = soeUsedFromFullWh ?? vcuUsedWh;
  const packRemainingWh = targetWh == null || packUsedForBudgetWh == null ? null : targetWh - packUsedForBudgetWh;
  const packUsedPercent = targetWh == null || packUsedForBudgetWh == null ? 0 : clamp((packUsedForBudgetWh / targetWh) * 100, 0, 100);
  const projectedNetWh = averages.avgEnergyWh != null && targetLaps > 0 ? averages.avgEnergyWh * targetLaps : null;
  const selectedAvgDelta = averages.avgEnergyWh != null && targetEnergyPerLapWh != null
    ? averages.avgEnergyWh - targetEnergyPerLapWh
    : null;

  return (
    <Panel title="Energy Strategy" icon={<Zap size={18} />}>
      <div className="energyPlan">
        <div className="energyHeroGrid">
          <div className="energyPlanHero">
            <span>Usable budget</span>
            <strong>{targetWh == null ? "--" : `${targetEnergyKwh.toFixed(2)} kWh`}</strong>
            <small>0 SOE floor: {soeCutoffCellV.toFixed(2)} V min-cell OCV. Nominal full pack: {PACK_ENERGY_KWH.toFixed(2)} kWh.</small>
          </div>
          <div className="energyPlanHero">
            <span>Pack used from SOE</span>
            <strong>{soeUsedFromFullWh == null ? "--" : formatKwhFromWh(soeUsedFromFullWh)}</strong>
            <small>{pack.soePercent == null ? "Waiting for car SOC." : `${pack.soePercent.toFixed(0)}% remaining (from car SOC).`}</small>
          </div>
        </div>
        <div className="energyPlanBar" aria-label="SOE energy estimate used versus usable pack budget">
          <span style={{ width: `${packUsedPercent}%` }} />
        </div>
        <div className="energyPlanGrid">
          <Metric label="Budget Remaining" value={packRemainingWh == null ? "--" : formatKwhFromWh(packRemainingWh)} />
          <Metric label="SOE Remaining" value={soeRemainingWh == null ? "--" : formatKwhFromWh(soeRemainingWh)} />
          <Metric label="Net Used (VCU)" value={formatKwhFromWh(vcuUsedWh)} />
          <Metric label="Net / Lap" value={targetEnergyPerLapWh == null ? "--" : `${targetEnergyPerLapWh.toFixed(0)} Wh`} />
          <Metric label="Drive (VCU)" value={formatKwhFromWh(state.totalEnergyOutWh)} />
          <Metric label="Regen (VCU)" value={formatKwhFromWh(state.totalEnergyInWh)} />
          <Metric label="Drive Power" value={drivePowerKw == null ? "--" : `${drivePowerKw.toFixed(2)} kW`} />
          <Metric label="Regen Power" value={regenPowerKw == null ? "--" : `${regenPowerKw.toFixed(2)} kW`} />
          <Metric label="Lap Net" value={`${state.lapEnergyWh.toFixed(1)} Wh`} />
          <Metric label="Lap Drive" value={`${state.lapEnergyOutWh.toFixed(1)} Wh`} />
          <Metric label="Lap Regen" value={`${state.lapEnergyInWh.toFixed(1)} Wh`} />
          <Metric label="Avg Net Lap" value={averages.avgEnergyWh == null ? "--" : `${averages.avgEnergyWh.toFixed(1)} Wh`} />
          <Metric label="Avg Regen" value={averages.avgEnergyInWh == null ? "--" : `${averages.avgEnergyInWh.toFixed(1)} Wh`} />
        </div>
        <small className="muted">
          {selectedAvgDelta != null
            ? `Selected laps are ${formatEnergyDelta(selectedAvgDelta)} per lap; projected total ${projectedNetWh == null ? "--" : formatKwhFromWh(projectedNetWh)}.`
            : `SOE gives a pack-from-full estimate; VCU projected total ${projectedNetWh == null ? "--" : formatKwhFromWh(projectedNetWh)} stays separate.`}
        </small>
      </div>
    </Panel>
  );
}

// G-Force panel: a live G-G plot driven by the chudpi IMU. The enricher computes
// `long_g` and `lat_g` from the chudpi's PIMU sentence (gps_imu[0]/[1]) — units
// are m/s², not g, despite the name. We convert here. The IMU lives on the
// chudpi GPS Pi; values include the static gravity offset until the user
// presses Zero on a level surface to subtract it.
function GForcePanel({ state }: { state: LiveSessionState }) {
  const [zero, setZero] = useState<{ long: number; lat: number }>(() => {
    try {
      const raw = localStorage.getItem("trackside-imu-zero");
      if (!raw) return { long: 0, lat: 0 };
      const v = JSON.parse(raw) as { long?: number; lat?: number };
      return { long: Number(v.long) || 0, lat: Number(v.lat) || 0 };
    } catch { return { long: 0, lat: 0 }; }
  });
  const sample = state.lastSample;
  const rawLong = Number(sample?.values?.long_g ?? 0);
  const rawLat = Number(sample?.values?.lat_g ?? 0);
  const hasImu = sample?.values != null && ("long_g" in sample.values || "lat_g" in sample.values);
  // m/s² → g, minus calibrated offset.
  const longG = (rawLong - zero.long) / 9.81;
  const latG = (rawLat - zero.lat) / 9.81;
  const totalG = Math.sqrt(longG * longG + latG * latG);
  // Trail of the last ~80 samples (≈8 s at 10 Hz).
  const points = useMemo(() => {
    const tail = state.samples.slice(-80);
    return tail.map((s) => ({
      x: (Number(s.values?.lat_g ?? 0) - zero.lat) / 9.81,
      y: (Number(s.values?.long_g ?? 0) - zero.long) / 9.81,
    }));
  }, [state.samples, zero]);
  const captureZero = () => {
    const z = { long: rawLong, lat: rawLat };
    setZero(z);
    localStorage.setItem("trackside-imu-zero", JSON.stringify(z));
  };
  const resetZero = () => {
    const z = { long: 0, lat: 0 };
    setZero(z);
    localStorage.setItem("trackside-imu-zero", JSON.stringify(z));
  };
  const SVG_SIZE = 200;
  const FULL_SCALE_G = 3;
  const scale = ((SVG_SIZE - 20) / 2) / FULL_SCALE_G;
  const cx = SVG_SIZE / 2;
  const cy = SVG_SIZE / 2;
  return (
    <Panel title="G-Forces" icon={<Target size={18} />}>
      <div className="ggBox">
        <svg viewBox={`0 0 ${SVG_SIZE} ${SVG_SIZE}`} className="ggChart" role="img" aria-label="Lateral vs longitudinal G plot">
          {[1, 2, 3].map((g) => (
            <circle key={g} cx={cx} cy={cy} r={scale * g} className="ggGridRing" />
          ))}
          <line x1={cx} y1={5} x2={cx} y2={SVG_SIZE - 5} className="ggGridLine" />
          <line x1={5} y1={cy} x2={SVG_SIZE - 5} y2={cy} className="ggGridLine" />
          {points.map((p, i) => (
            <circle
              key={i}
              cx={cx + p.x * scale}
              cy={cy - p.y * scale}
              r={1.6}
              className="ggTrailDot"
              opacity={((i + 1) / points.length) * 0.7}
            />
          ))}
          {hasImu ? (
            <circle cx={cx + latG * scale} cy={cy - longG * scale} r={5} className="ggLiveDot" />
          ) : null}
          <text x={cx + 3} y={11} className="ggAxisLabel">+Long</text>
          <text x={cx + 3} y={SVG_SIZE - 4} className="ggAxisLabel">−Long</text>
          <text x={SVG_SIZE - 4} y={cy - 3} textAnchor="end" className="ggAxisLabel">Lat→</text>
          <text x={5} y={cy - 3} className="ggAxisLabel">←Lat</text>
        </svg>
        <div className="ggSidebar">
          <Metric label="Long" value={hasImu ? `${longG.toFixed(2)} g` : "--"} />
          <Metric label="Lat" value={hasImu ? `${latG.toFixed(2)} g` : "--"} />
          <Metric label="Total" value={hasImu ? `${totalG.toFixed(2)} g` : "--"} />
          <div className="ggButtons">
            <button className="tool" disabled={!hasImu} onClick={captureZero} title="Capture current IMU reading as the static zero offset. Press while the car is stationary on level ground.">
              <Target size={13} /> Zero
            </button>
            {zero.long !== 0 || zero.lat !== 0 ? (
              <button className="tool" onClick={resetZero} title="Clear the zero offset and show raw IMU values">
                Reset
              </button>
            ) : null}
          </div>
        </div>
      </div>
      <small className="muted">
        IMU lives on the chudpi GPS Pi. Raw values include the static gravity component from the IMU mount tilt —
        press <strong>Zero</strong> on a level surface to subtract it.
      </small>
    </Panel>
  );
}

function DriverControlsPanel({ sample, torqueParamSet }: { sample: LiveSample | null; torqueParamSet: VcuTorqueParamSet }) {
  const controls = driverControls(sample, torqueParamSet);
  return (
    <Panel title="Driver Controls" icon={<Disc3 size={18} />}>
      <div className="driverControls">
        <div className="steeringReadout">
          <Disc3
            size={82}
            strokeWidth={1.7}
            style={{ transform: `rotate(${controls.steeringAngleDeg ?? 0}deg)` }}
            aria-label="Steering angle"
          />
          <strong>{controls.steeringAngleDeg == null ? "--" : `${controls.steeringAngleDeg.toFixed(1)} deg`}</strong>
        </div>
        <div className="pedalBars">
          <div className="accelControl">
            <VerticalControlBar label="Accel" value={controls.throttlePercent} max={100} unit="%" tone="throttle" />
            <div className="torqueRequestReadout">
              <span>Req torque</span>
              <strong>{controls.requestedTorqueNm == null ? "--" : formatSignedTorque(controls.requestedTorqueNm)}</strong>
              <small>{controls.requestedTorqueSource}</small>
            </div>
          </div>
          <VerticalControlBar label="front" value={controls.bse1Psi} max={3000} unit="psi" tone="brake" />
          <VerticalControlBar label="rear-1" value={controls.bse2Psi} max={3000} unit="psi" tone="brake" />
          <VerticalControlBar label="rear-2" value={controls.bse3Psi} max={3000} unit="psi" tone="brake" />
        </div>
      </div>
    </Panel>
  );
}

function VerticalControlBar({
  label,
  value,
  max,
  unit,
  tone,
}: {
  label: string;
  value: number | null;
  max: number;
  unit: string;
  tone: "throttle" | "brake";
}) {
  const percent = value == null ? 0 : clamp(value / max * 100, 0, 100);
  const display = value == null
    ? "--"
    : unit === "%"
      ? `${value.toFixed(0)}%`
      : `${Math.round(value).toLocaleString()} psi`;
  return (
    <div className="controlBar">
      <span>{label}</span>
      <div className={`controlTrack ${tone}`}>
        <span style={{ height: `${percent}%` }} />
      </div>
      <strong>{display}</strong>
    </div>
  );
}

function TempsStatusPanel({ sample }: { sample: LiveSample | null }) {
  const temps = tempStatus(sample);
  return (
    <Panel title="Temps" icon={<Thermometer size={18} />}>
      <div className="tempMetricGrid">
        <Metric label="Ambient" value={temps.ambient == null ? "--" : `${temps.ambient.toFixed(1)} C`} />
        <Metric label="Coolant" value={temps.coolant == null ? "--" : `${temps.coolant.toFixed(1)} C`} />
        <Metric label="Fan RPM" value={temps.fanRpm == null ? "--" : `${Math.round(temps.fanRpm).toLocaleString()} rpm`} />
        <Metric label="Motor Temp" value={temps.motor == null ? "--" : `${temps.motor.toFixed(1)} C`} />
        <Metric label="Inverter Temp" value={temps.inverter == null ? "--" : `${temps.inverter.toFixed(1)} C`} />
      </div>
    </Panel>
  );
}

function EnergyWindowChart({
  state,
  windowS,
  onWindowS,
}: {
  state: LiveSessionState;
  windowS: number;
  onWindowS: Dispatch<SetStateAction<number>>;
}) {
  const trace = energyTrace(state.samples, windowS);
  const width = 900;
  const height = 160;
  const padLeft = 46;
  const padRight = 18;
  const padTop = 14;
  const padBottom = 28;
  const plotW = width - padLeft - padRight;
  const plotH = height - padTop - padBottom;
  const lastT = state.lastSample?.t ?? Date.now();
  const startT = lastT - windowS * 1000;
  const maxEnergy = Math.max(1, ...trace.flatMap((point) => [point.energyOutWh, point.energyInWh, Math.max(0, point.energyWh)]));
  const outPoints = trace
    .map((point) => {
      const x = padLeft + Math.max(0, Math.min(1, (point.t - startT) / (windowS * 1000))) * plotW;
      const y = padTop + (1 - Math.max(0, Math.min(1, point.energyOutWh / maxEnergy))) * plotH;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(" ");
  const inPoints = trace
    .map((point) => {
      const x = padLeft + Math.max(0, Math.min(1, (point.t - startT) / (windowS * 1000))) * plotW;
      const y = padTop + (1 - Math.max(0, Math.min(1, point.energyInWh / maxEnergy))) * plotH;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(" ");
  const lapBreaks = state.laps.filter((lap) => lap.endMs >= startT && lap.endMs <= lastT);
  const latest = trace.at(-1);
  const latestEnergy = latest?.energyWh ?? 0;
  return (
    <div className="energyWindow">
      <div className="energyToolbar">
        <Metric label="Window Out" value={`${(latest?.energyOutWh ?? 0).toFixed(1)} Wh`} />
        <Metric label="Regen In" value={`${(latest?.energyInWh ?? 0).toFixed(1)} Wh`} tone="good" />
        <Metric label="Net" value={`${latestEnergy.toFixed(1)} Wh`} />
        <label>
          <span>Window</span>
          <select value={windowS} onChange={(event) => onWindowS(Number(event.target.value))}>
            <option value={10}>10 s</option>
            <option value={30}>30 s</option>
            <option value={60}>1 min</option>
            <option value={120}>2 min</option>
            <option value={300}>5 min</option>
          </select>
        </label>
      </div>
      <svg className="energyChart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Energy over selected time window">
        <rect x={padLeft} y={padTop} width={plotW} height={plotH} rx="6" />
        <line x1={padLeft} x2={width - padRight} y1={height - padBottom} y2={height - padBottom} />
        <line x1={padLeft} x2={padLeft} y1={padTop} y2={height - padBottom} />
        <text x={padLeft} y={height - 7}>-{windowS}s</text>
        <text x={width - padRight - 28} y={height - 7}>now</text>
        <text x={6} y={padTop + 10}>{maxEnergy.toFixed(1)} Wh</text>
        <text x={10} y={height - padBottom}>0 Wh</text>
        {outPoints ? <polyline className="energyOutLine" points={outPoints} /> : null}
        {inPoints ? <polyline className="energyInLine" points={inPoints} /> : null}
        {lapBreaks.map((lap) => {
          const x = padLeft + Math.max(0, Math.min(1, (lap.endMs - startT) / (windowS * 1000))) * plotW;
          return (
            <g key={lap.id} className="lapBreak">
              <line x1={x} x2={x} y1={padTop} y2={height - padBottom} />
              <text x={x + 4} y={padTop + 13}>{lap.label}</text>
            </g>
          );
        })}
      </svg>
    </div>
  );
}

function TemperatureWindowChart({ state, windowS }: { state: LiveSessionState; windowS: number }) {
  const series = temperatureSeries(state.samples, windowS);
  const width = 900;
  const height = 160;
  const padLeft = 46;
  const padRight = 18;
  const padTop = 14;
  const padBottom = 28;
  const plotW = width - padLeft - padRight;
  const plotH = height - padTop - padBottom;
  const lastT = state.lastSample?.t ?? Date.now();
  const startT = lastT - windowS * 1000;
  const allValues = series.flatMap((item) => item.segments.flatMap((segment) => segment.map((point) => point.value)));
  const minTemp = allValues.length ? Math.floor(Math.min(...allValues) / 5) * 5 : 0;
  const maxTemp = allValues.length ? Math.ceil(Math.max(...allValues) / 5) * 5 : 100;
  const span = Math.max(10, maxTemp - minTemp);
  const yFor = (value: number) => padTop + (1 - clamp((value - minTemp) / span, 0, 1)) * plotH;
  const xFor = (t: number) => padLeft + clamp((t - startT) / (windowS * 1000), 0, 1) * plotW;
  return (
    <div className="energyWindow">
      <div className="tempLegend">
        {series.map((item) => (
          <span key={item.key}>
            <i style={{ background: item.color }} />
            {item.label}
          </span>
        ))}
      </div>
      <svg className="energyChart tempChart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Temperatures over selected time window">
        <rect x={padLeft} y={padTop} width={plotW} height={plotH} rx="6" />
        <line x1={padLeft} x2={width - padRight} y1={height - padBottom} y2={height - padBottom} />
        <line x1={padLeft} x2={padLeft} y1={padTop} y2={height - padBottom} />
        <text x={padLeft} y={height - 7}>-{windowS}s</text>
        <text x={width - padRight - 28} y={height - 7}>now</text>
        <text x={7} y={padTop + 10}>{maxTemp.toFixed(0)} C</text>
        <text x={7} y={height - padBottom}>{minTemp.toFixed(0)} C</text>
        {series.flatMap((item) => item.segments.map((segment, index) => {
          const points = segment.map((point) => `${xFor(point.t).toFixed(1)},${yFor(point.value).toFixed(1)}`).join(" ");
          return points ? <polyline key={`${item.key}-${index}`} points={points} style={{ stroke: item.color }} /> : null;
        }))}
      </svg>
    </div>
  );
}

function LiveDataPanel({ state }: { state: LiveSessionState }) {
  const sample = state.lastSample;
  const values = sample?.values ?? {};
  const dcBusV = dcBusVoltageFor(sample);
  const energyV = energyVoltageFor(sample);
  const dcBusCurrent = dcBusCurrentFor(sample);
  const signedPowerKw = signedVcuPowerKwFor(state.previousSample, sample);
  const drivePowerKw = signedPowerKw == null ? null : Math.max(0, signedPowerKw);
  const regenPowerKw = signedPowerKw == null ? null : Math.max(0, -signedPowerKw);
  const motorRpm = firstLiveValue(values, ["controls_motor_speed", "motor_speed", "dynamics_inverter_rpm", "inverter_rpm"]);
  const rawApps = rawAppsTravelDetails(values);
  const rawRows = [...rawAppsLiveRows(rawApps), ...topLiveValues(values)];
  return (
    <div className="liveDataPanel">
      <div className="liveDataGrid">
        <Metric label="Samples" value={state.samples.length.toLocaleString()} />
        <Metric label="Last Sample" value={sample ? formatTime(sample.t) : "--"} />
        <Metric label="Drive Power" value={drivePowerKw == null ? "--" : `${drivePowerKw.toFixed(2)} kW`} />
        <Metric label="Regen Power" value={regenPowerKw == null ? "--" : `${regenPowerKw.toFixed(2)} kW`} tone="good" />
        <Metric label={dcBusV == null && energyV != null ? "Est V / I" : "DC Bus"} value={energyV == null || dcBusCurrent == null ? "--" : `${energyV.toFixed(1)} V / ${dcBusCurrent.toFixed(1)} A`} />
        <Metric label="Power Src" value="VCU 0x1C9" />
        <Metric label="Motor RPM" value={motorRpm == null ? "--" : `${Math.round(motorRpm).toLocaleString()} rpm`} />
        <Metric label="Raw APPS" value={rawApps == null ? "--" : `${(rawApps.average * 100).toFixed(1)}%`} />
        <Metric label="Raw Channels" value={Object.keys(values).length.toLocaleString()} />
      </div>
      {sample ? (
        <div className="rawValueList">
          {rawRows.map(([key, value]) => (
            <div key={key} className="rawValueRow">
              <span>{labelFromKey(key)}</span>
              <strong>{formatRawLiveValue(key, value)}</strong>
            </div>
          ))}
          {!rawRows.length ? <small className="muted">Sample arrived, but it did not contain numeric channels.</small> : null}
        </div>
      ) : (
        <small className="muted">
          {state.connected
            ? state.status.includes("MQTT")
              ? "MQTT link is up. No car samples have arrived on this topic yet."
              : "Broker link is up. No car samples have arrived on this topic yet."
            : "Start Live to watch broker samples and raw channels."}
        </small>
      )}
    </div>
  );
}

function LiveLapTable({
  laps,
  bestLap,
  bestSectors,
  currentSectors,
  currentLapElapsedMs,
  currentLapEnergyWh,
  currentLapEnergyOutWh,
  currentLapEnergyInWh,
  sectorCount,
  selectedLapIds,
  onToggleLap,
  onEditLapTime,
  onEditLapNote,
  onDeleteLap,
  editable,
  targetEnergyPerLapWh,
  averages,
}: {
  laps: LiveLap[];
  bestLap: LiveLap | null;
  bestSectors: Array<number | null>;
  currentSectors: number[];
  currentLapElapsedMs: number;
  currentLapEnergyWh: number;
  currentLapEnergyOutWh: number;
  currentLapEnergyInWh: number;
  sectorCount: number;
  selectedLapIds: Set<string>;
  onToggleLap: (lapId: string) => void;
  onEditLapTime: (lapId: string, durationMs: number) => void;
  onEditLapNote: (lapId: string, notes: string) => void;
  onDeleteLap: (lapId: string) => void;
  editable: boolean;
  targetEnergyPerLapWh: number | null;
  averages: {
    count: number;
    avgMs: number | null;
    avgEnergyWh: number | null;
    avgEnergyOutWh: number | null;
    avgEnergyInWh: number | null;
  };
}) {
  const columns = sectorCount;
  const totalCols = 3 + columns + 3 + (targetEnergyPerLapWh != null ? 1 : 0) + 1; // +1 = Notes
  const currentTargetEnergyWh = currentLapEnergyWh;
  const averageTargetEnergyWh = averages.avgEnergyWh;
  const tableWrapRef = useRef<HTMLDivElement | null>(null);
  const lastLap = laps[laps.length - 1] ?? null;
  const lastLapTargetEnergyWh = lastLap == null ? null : lastLap.energyWh;
  const lastLapDeltaWh = targetEnergyPerLapWh != null && lastLapTargetEnergyWh != null ? lastLapTargetEnergyWh - targetEnergyPerLapWh : null;
  const averageDeltaWh = targetEnergyPerLapWh != null && averageTargetEnergyWh != null ? averageTargetEnergyWh - targetEnergyPerLapWh : null;

  useEffect(() => {
    const table = tableWrapRef.current;
    if (!table) return;
    table.scrollTop = table.scrollHeight;
  }, [laps.length]);

  return (
    <>
      <div ref={tableWrapRef} className="lapTableWrap">
        <table className="lapTable">
          <thead>
            <tr>
              <th aria-label="Include in average" />
              <th>Lap</th>
              <th>Time</th>
              {Array.from({ length: columns }, (_sector, index) => <th key={`sector-head-${index}`}>S{index + 1}</th>)}
              <th>Drive</th>
              <th>Regen</th>
              <th>Net</th>
              {targetEnergyPerLapWh != null ? <th>Δ Net</th> : null}
              <th>Notes</th>
            </tr>
          </thead>
          <tbody>
            {laps.map((lap) => {
              const selected = selectedLapIds.has(lap.id);
              const targetEnergyWh = lap.energyWh;
              const delta = targetEnergyPerLapWh != null ? targetEnergyWh - targetEnergyPerLapWh : null;
              return (
                <tr key={lap.id} className={selected ? "" : "lapDeselected"}>
                  <td>
                    <input
                      type="checkbox"
                      checked={selected}
                      onChange={() => onToggleLap(lap.id)}
                      aria-label={`Include ${lap.label} in average`}
                    />
                  </td>
                  <td className={bestLap?.id === lap.id ? "purpleText" : ""}>
                    {lap.label}
                    {editable ? (
                      <button
                        onClick={() => onDeleteLap(lap.id)}
                        title={`Delete ${lap.label} (accident / bad data)`}
                        aria-label={`Delete ${lap.label}`}
                        style={{ marginLeft: 6, background: "none", border: "none", color: "var(--muted-text)", cursor: "pointer", padding: 0, verticalAlign: "middle" }}
                      >
                        <Trash2 size={12} />
                      </button>
                    ) : null}
                  </td>
                  <td className={bestLap?.id === lap.id ? "purpleText" : ""}>
                    <EditableLapTime
                      durationMs={lap.durationMs}
                      editable={editable}
                      className={bestLap?.id === lap.id ? "purpleText" : ""}
                      onCommit={(ms) => onEditLapTime(lap.id, ms)}
                    />
                  </td>
                  {Array.from({ length: columns }, (_unused, index) => {
                    const bestSector = bestSectors[index];
                    return (
                    <td key={`${lap.id}-sector-${index}`} className={bestSector != null && lap.sectors[index] === bestSector ? "purpleText" : ""}>
                      {lap.sectors[index] == null ? "--" : formatLapTime(lap.sectors[index])}
                    </td>
                    );
                  })}
                  <td>{lapEnergyOutWh(lap).toFixed(1)} Wh</td>
                  <td className="goodText">{lapEnergyInWh(lap).toFixed(1)} Wh</td>
                  <td>{lap.energyWh.toFixed(1)} Wh</td>
                  {targetEnergyPerLapWh != null ? <td className={delta != null && delta > 0 ? "deltaOver" : "deltaUnder"}>{delta == null ? "--" : formatEnergyDelta(delta)}</td> : null}
                  <td>
                    {editable ? (
                      <input
                        defaultValue={lap.notes ?? ""}
                        placeholder="note…"
                        onBlur={(e) => { if ((e.target.value ?? "") !== (lap.notes ?? "")) onEditLapNote(lap.id, e.target.value); }}
                        onKeyDown={(e) => { if (e.key === "Enter") (e.target as HTMLInputElement).blur(); }}
                        aria-label={`Note for ${lap.label}`}
                        style={{ width: 130, font: "inherit", background: "var(--surface-alt)", border: "1px solid var(--border)", borderRadius: 4, color: "var(--text)", padding: "1px 5px" }}
                      />
                    ) : (lap.notes || "")}
                  </td>
                </tr>
              );
            })}
            {!laps.length && currentLapElapsedMs <= 0 ? (
              <tr>
                <td colSpan={totalCols}>No completed flying laps yet. Out lap and in lap are excluded from best lap.</td>
              </tr>
            ) : null}
            {currentLapElapsedMs > 0 ? (
              <tr className="currentLapRow">
                <td>
                  <span className="lapCurrentDot" aria-hidden="true" />
                </td>
                <td>Current</td>
                <td>{formatLapTime(currentLapElapsedMs)}</td>
                {Array.from({ length: columns }, (_unused, index) => (
                  <td key={`current-sector-${index}`}>{currentSectors[index] == null ? "--" : formatLapTime(currentSectors[index])}</td>
                ))}
                <td>{currentLapEnergyOutWh.toFixed(1)} Wh</td>
                <td className="goodText">{currentLapEnergyInWh.toFixed(1)} Wh</td>
                <td>{currentLapEnergyWh.toFixed(1)} Wh</td>
                {targetEnergyPerLapWh != null ? <td>{formatEnergyDelta(currentTargetEnergyWh - targetEnergyPerLapWh)}</td> : null}
                <td />
              </tr>
            ) : null}
          </tbody>
          {averages.count > 0 ? (
            <tfoot>
              <tr className="lapAverageRow">
                <td />
                <td>Avg ({averages.count})</td>
                <td>{averages.avgMs == null ? "--" : formatLapTime(averages.avgMs)}</td>
                {Array.from({ length: columns }, (_unused, index) => <td key={`avg-sector-${index}`} />)}
                <td>{averages.avgEnergyOutWh == null ? "--" : `${averages.avgEnergyOutWh.toFixed(1)} Wh`}</td>
                <td className="goodText">{averages.avgEnergyInWh == null ? "--" : `${averages.avgEnergyInWh.toFixed(1)} Wh`}</td>
                <td>{averages.avgEnergyWh == null ? "--" : `${averages.avgEnergyWh.toFixed(1)} Wh`}</td>
                {targetEnergyPerLapWh != null ? (
                  <td>{averageTargetEnergyWh == null ? "--" : formatEnergyDelta(averageTargetEnergyWh - targetEnergyPerLapWh)}</td>
                ) : null}
                <td />
              </tr>
            </tfoot>
          ) : null}
        </table>
      </div>
      <div className="lapSummaryStack">
        <div className="lapMiniPane">
          <div className="lapMiniHeader">
            <span>Last lap</span>
            <strong>{lastLap == null ? "--" : lastLap.label}</strong>
          </div>
          <div className="lapMiniGrid">
            <Metric label="Time" value={lastLap == null ? "--" : formatLapTime(lastLap.durationMs)} />
            <Metric label="Net" value={lastLap == null ? "--" : `${lastLap.energyWh.toFixed(1)} Wh`} />
            <Metric label="Drive" value={lastLap == null ? "--" : `${lapEnergyOutWh(lastLap).toFixed(1)} Wh`} />
            <Metric label="Regen" value={lastLap == null ? "--" : `${lapEnergyInWh(lastLap).toFixed(1)} Wh`} />
            <Metric label="Target Δ" value={lastLapDeltaWh == null ? "--" : formatEnergyDelta(lastLapDeltaWh)} />
          </div>
        </div>
        <div className="lapMiniPane">
          <div className="lapMiniHeader">
            <span>Selected average</span>
            <strong>{averages.count ? `${averages.count} laps` : "--"}</strong>
          </div>
          <div className="lapMiniGrid">
            <Metric label="Time" value={averages.avgMs == null ? "--" : formatLapTime(averages.avgMs)} />
            <Metric label="Net" value={averages.avgEnergyWh == null ? "--" : `${averages.avgEnergyWh.toFixed(1)} Wh`} />
            <Metric label="Drive" value={averages.avgEnergyOutWh == null ? "--" : `${averages.avgEnergyOutWh.toFixed(1)} Wh`} />
            <Metric label="Regen" value={averages.avgEnergyInWh == null ? "--" : `${averages.avgEnergyInWh.toFixed(1)} Wh`} />
            <Metric label="Target Δ" value={averageDeltaWh == null ? "--" : formatEnergyDelta(averageDeltaWh)} />
          </div>
        </div>
      </div>
    </>
  );
}

function formatEnergyDelta(deltaWh: number) {
  const sign = deltaWh > 0 ? "+" : "";
  return `${sign}${deltaWh.toFixed(1)} Wh`;
}

function formatKwhFromWh(wh: number) {
  const sign = wh < 0 ? "-" : "";
  return `${sign}${(Math.abs(wh) / 1000).toFixed(2)} kWh`;
}

function formatSignedTorque(torqueNm: number) {
  const sign = torqueNm > 0 ? "+" : "";
  return `${sign}${torqueNm.toFixed(1)} Nm`;
}

function defaultSelectedLapIds(laps: LiveLap[], savedSelectedIds?: string[], selectionSaved = false) {
  const lapIds = new Set(laps.map((lap) => lap.id));
  const validSavedIds = (savedSelectedIds ?? []).filter((id) => lapIds.has(id));
  if (selectionSaved) return new Set(validSavedIds);
  return new Set(validSavedIds.length ? validSavedIds : laps.map((lap) => lap.id));
}

function normalizeSavedLiveLap(lap: LiveLap) {
  return {
    ...lap,
    energyOutWh: lapEnergyOutWh(lap),
    energyInWh: lapEnergyInWh(lap),
  };
}

function lapEnergyOutWh(lap: LiveLap) {
  return lap.energyOutWh ?? Math.max(0, lap.energyWh);
}

function lapEnergyInWh(lap: LiveLap) {
  return lap.energyInWh ?? 0;
}

function TrackManagerPanel({
  track,
  tracks,
  hasStartFinish,
  gateDrawMode,
  onSetTrack,
  onNewTrack,
  onSetGateDrawMode,
  onSaveTrack,
  onUploadTrack,
  onDownloadTrack,
  onUpdateGate,
  onMoveSplitGate,
  onRemoveGate,
}: {
  track: TrackDefinition;
  tracks: TrackDefinition[];
  hasStartFinish: boolean;
  gateDrawMode: GateDrawMode;
  onSetTrack: Dispatch<SetStateAction<TrackDefinition>>;
  onNewTrack: () => void;
  onSetGateDrawMode: Dispatch<SetStateAction<GateDrawMode>>;
  onSaveTrack: () => void;
  onUploadTrack: (event: ChangeEvent<HTMLInputElement>) => void;
  onDownloadTrack: () => void;
  onUpdateGate: (index: number, patch: Partial<GateLine>) => void;
  onMoveSplitGate: (index: number, direction: -1 | 1) => void;
  onRemoveGate: (index: number) => void;
}) {
  return (
    <Panel title="Track Split JSON" icon={<Flag size={18} />}>
      <div className="trackForm">
        <input value={track.name} onChange={(e) => onSetTrack({ ...track, name: e.target.value })} />
        <select
          value={track.slug}
          onChange={(e) => onSetTrack(normalizeTrack(tracks.find((t) => t.slug === e.target.value) ?? track))}
        >
          <option value={track.slug}>{track.name}</option>
          {tracks.filter((t) => t.slug !== track.slug).map((t) => (
            <option key={t.slug} value={t.slug}>{t.name}</option>
          ))}
        </select>
        <textarea value={track.notes} placeholder="Track notes" onChange={(e) => onSetTrack({ ...track, notes: e.target.value })} />
      </div>
      <div className="gateButtons">
        <button className="tool" onClick={onNewTrack}><Plus size={15} /> New</button>
        <button
          className={gateDrawMode === "start_finish" ? "tool activeTool" : "tool"}
          disabled={hasStartFinish}
          onClick={() => onSetGateDrawMode((mode) => (mode === "start_finish" ? null : "start_finish"))}
        >
          <Plus size={15} /> Start
        </button>
        <button
          className={gateDrawMode === "split" ? "tool activeTool" : "tool"}
          onClick={() => onSetGateDrawMode((mode) => (mode === "split" ? null : "split"))}
        >
          <Plus size={15} /> Split
        </button>
        <button className="tool" onClick={onSaveTrack}><Save size={15} /> Save</button>
        <label className="tool fileTool">
          <Upload size={15} /> Upload
          <input type="file" accept="application/json,.json" onChange={onUploadTrack} />
        </label>
        <button className="tool" onClick={onDownloadTrack}><Download size={15} /> Download</button>
      </div>
      <div className="gateList">
        <small className="muted">
          {gateDrawMode ? "Drag a line across the GPS map to place the selected gate." : "Select Start or Split, then draw the gate on the GPS map."}
        </small>
        {track.gates.map((gate, index) => {
          const splitIndexes = track.gates.flatMap((item, itemIndex) => (item.role === "split" ? [itemIndex] : []));
          const splitPosition = splitIndexes.indexOf(index);
          return (
            <div key={gate.id} className="gate gateEditor">
              <div className="gateMain">
                <input
                  className="gateName"
                  value={gate.label}
                  aria-label={`${gate.role.replace("_", " ")} label`}
                  onChange={(e) => onUpdateGate(index, { label: e.target.value })}
                />
                <span className={gate.role === "start_finish" ? "rolePill startRole" : "rolePill"}>{gate.role === "start_finish" ? "Start Finish" : "Split"}</span>
              </div>
              <div className="gateActions">
                {gate.role === "split" ? (
                  <>
                    <button className="miniTool" disabled={splitPosition <= 0} onClick={() => onMoveSplitGate(index, -1)} aria-label={`Move ${gate.label} up`}>
                      <ArrowUp size={14} />
                    </button>
                    <button className="miniTool" disabled={splitPosition < 0 || splitPosition >= splitIndexes.length - 1} onClick={() => onMoveSplitGate(index, 1)} aria-label={`Move ${gate.label} down`}>
                      <ArrowDown size={14} />
                    </button>
                  </>
                ) : null}
                <button className="miniTool dangerTool" onClick={() => onRemoveGate(index)} aria-label={`Remove ${gate.label}`}>
                  <Trash2 size={14} />
                </button>
              </div>
            </div>
          );
        })}
      </div>
    </Panel>
  );
}

function TelemetryChart({
  points,
  unit,
  range,
  segments,
  previewSegmentIds,
  threshold,
  onRange,
}: {
  points: SeriesPoint[];
  unit: string;
  range: [number, number] | null;
  segments: SegmentSummary[];
  previewSegmentIds: Set<string>;
  threshold: number;
  onRange: (range: [number, number] | null) => void;
}) {
  const [dragStart, setDragStart] = useState<number | null>(null);
  const width = 1100;
  const height = 330;
  const pad = 42;
  const finite = points.filter((p) => typeof p.v === "number") as Array<{ t: number; v: number }>;
  const minT = finite[0]?.t ?? 0;
  const maxT = finite.at(-1)?.t ?? minT + 1;
  const values = finite.map((p) => p.v).concat([threshold]);
  const minV = values.length ? Math.min(...values) : 0;
  const maxV = values.length ? Math.max(...values) : 1;
  const x = (t: number) => pad + ((t - minT) / Math.max(1, maxT - minT)) * (width - pad * 2);
  const y = (v: number) => height - pad - ((v - minV) / Math.max(1, maxV - minV)) * (height - pad * 2);
  const toT = (clientX: number, rect: DOMRect) => {
    const local = Math.max(pad, Math.min(width - pad, ((clientX - rect.left) / rect.width) * width));
    return Math.round(minT + ((local - pad) / (width - pad * 2)) * (maxT - minT));
  };
  const line = finite.map((p) => `${x(p.t)},${y(p.v)}`).join(" ");
  const xTicks = Array.from({ length: 6 }, (_, i) => minT + ((maxT - minT) * i) / 5);
  const yTicks = Array.from({ length: 5 }, (_, i) => minV + ((maxV - minV) * i) / 4);
  const visibleSegments = segments.filter((segment) => segment.end_ms >= minT && segment.start_ms <= maxT);

  return (
    <svg
      className="chart"
      viewBox={`0 0 ${width} ${height}`}
      onMouseDown={(e) => setDragStart(toT(e.clientX, e.currentTarget.getBoundingClientRect()))}
      onMouseMove={(e) => {
        if (dragStart == null) return;
        const t = toT(e.clientX, e.currentTarget.getBoundingClientRect());
        onRange([Math.min(dragStart, t), Math.max(dragStart, t)]);
      }}
      onMouseUp={() => setDragStart(null)}
      onMouseLeave={() => setDragStart(null)}
    >
      <rect x="0" y="0" width={width} height={height} rx="8" />
      {visibleSegments.map((segment) => (
        <rect
          key={segment.id}
          className={previewSegmentIds.has(segment.id) ? "autoSegment previewSegment" : "autoSegment"}
          x={x(segment.start_ms)}
          y={pad}
          width={Math.max(1, x(segment.end_ms) - x(segment.start_ms))}
          height={height - pad * 2}
        />
      ))}
      {yTicks.map((tick, index) => (
        <g key={`y-${index}-${tick}`}>
          <line x1={pad} x2={width - pad} y1={y(tick)} y2={y(tick)} />
          <text x={pad + 6} y={y(tick) + 4}>{formatValue(tick)} {unit}</text>
        </g>
      ))}
      {xTicks.map((tick, index) => (
        <g key={`x-${index}-${tick}`}>
          <line className="xTick" x1={x(tick)} x2={x(tick)} y1={height - pad} y2={height - pad + 6} />
          <text x={x(tick)} y={height - 12} textAnchor="middle">{formatTime(tick)}</text>
        </g>
      ))}
      <line className="thresholdLine" x1={pad} x2={width - pad} y1={y(threshold)} y2={y(threshold)} />
      {range ? <rect className="selection" x={x(range[0])} y={pad} width={Math.max(1, x(range[1]) - x(range[0]))} height={height - pad * 2} /> : null}
      <polyline points={line} />
    </svg>
  );
}

function SessionGpsBadge({ segment }: { segment: SegmentSummary }) {
  if (!segment.has_gps) return null;
  const pointText = segment.gps_points === 1 ? "1 GPS point" : `${segment.gps_points.toLocaleString()} GPS points`;
  return (
    <span className="sessionGpsBadge" title={pointText} aria-label={pointText}>
      <MapPinned size={14} />
    </span>
  );
}

function GpsTrace({
  points,
  gates,
  drawMode,
  onDrawGate,
  nextSplitNumber,
}: {
  points: GpsPoint[];
  gates: GateLine[];
  drawMode: GateDrawMode;
  onDrawGate: (gate: GateLine) => void;
  nextSplitNumber: number;
}) {
  const [drawStart, setDrawStart] = useState<{ x: number; y: number } | null>(null);
  const [drawEnd, setDrawEnd] = useState<{ x: number; y: number } | null>(null);
  const width = 760;
  const height = 520;
  const pad = 24;
  if (points.length < 2) {
    return (
      <div className="noGps">
        <strong>No GPS data in the selected session preview</strong>
        <span>Choose a session with GPS samples, or select a different day/channel range.</span>
      </div>
    );
  }
  const projected = points.map((p) => ({ ...p, ...project(p.lat, p.lon) }));
  const bounds = mapBounds(projected, width, height, pad);
  const line = projected.map((p) => `${bounds.x(p.mx)},${bounds.y(p.my)}`).join(" ");
  const tiles = satelliteTiles(bounds);
  const pointer = (event: MouseEvent<SVGSVGElement>) => {
    const rect = event.currentTarget.getBoundingClientRect();
    return {
      x: ((event.clientX - rect.left) / rect.width) * width,
      y: ((event.clientY - rect.top) / rect.height) * height,
    };
  };
  const finishDraw = (end: { x: number; y: number }) => {
    if (!drawMode || !drawStart) return;
    const dx = end.x - drawStart.x;
    const dy = end.y - drawStart.y;
    if (Math.hypot(dx, dy) < 8) return;
    const a = bounds.latLon(drawStart.x, drawStart.y);
    const b = bounds.latLon(end.x, end.y);
    onDrawGate({
      id: `${drawMode}-${Date.now()}`,
      label: drawMode === "start_finish" ? "Start Finish" : `Split ${nextSplitNumber}`,
      role: drawMode,
      lat1: a.lat,
      lon1: a.lon,
      lat2: b.lat,
      lon2: b.lon,
    });
  };
  return (
    <svg
      className={drawMode ? "map drawing" : "map"}
      viewBox={`0 0 ${width} ${height}`}
      onMouseDown={(event) => {
        if (!drawMode) return;
        const start = pointer(event);
        setDrawStart(start);
        setDrawEnd(start);
      }}
      onMouseMove={(event) => {
        if (!drawStart) return;
        setDrawEnd(pointer(event));
      }}
      onMouseUp={(event) => {
        const end = pointer(event);
        finishDraw(end);
        setDrawStart(null);
        setDrawEnd(null);
      }}
      onMouseLeave={() => {
        setDrawStart(null);
        setDrawEnd(null);
      }}
    >
      <rect x="0" y="0" width={width} height={height} rx="8" />
      {tiles.map((tile) => (
        <image key={`${tile.z}-${tile.x}-${tile.y}`} href={tile.url} x={bounds.x(tile.left)} y={bounds.y(tile.top)} width={bounds.scale * tile.size} height={bounds.scale * tile.size} preserveAspectRatio="none" />
      ))}
      <polyline points={line} />
      {gates.map((gate) => {
        const a = project(gate.lat1, gate.lon1);
        const b = project(gate.lat2, gate.lon2);
        const labelX = bounds.x((a.mx + b.mx) / 2);
        const labelY = bounds.y((a.my + b.my) / 2);
        return (
          <g key={gate.id}>
            <line className={gate.role === "start_finish" ? "startGate" : "splitGate"} x1={bounds.x(a.mx)} y1={bounds.y(a.my)} x2={bounds.x(b.mx)} y2={bounds.y(b.my)} />
            <text
              className={gate.role === "start_finish" ? "gateLabel startGateLabel" : "gateLabel splitGateLabel"}
              x={labelX + 6}
              y={labelY - 6}
            >
              {gate.label}
            </text>
          </g>
        );
      })}
      {drawStart && drawEnd ? (
        <line
          className={drawMode === "start_finish" ? "startGate drawingGate" : "splitGate drawingGate"}
          x1={drawStart.x}
          y1={drawStart.y}
          x2={drawEnd.x}
          y2={drawEnd.y}
        />
      ) : null}
      <text className="mapCredit" x={width - 10} y={height - 10} textAnchor="end">Esri World Imagery</text>
    </svg>
  );
}

function TrackBuilderMap({
  points,
  liveSample,
  gates,
  drawMode,
  onDrawGate,
  center,
  onCenter,
  targetSpanM,
  gpsUnavailable = false,
}: {
  points: GpsPoint[];
  liveSample: LiveSample | null;
  gates: GateLine[];
  drawMode: GateDrawMode;
  onDrawGate: (gate: GateLine) => void;
  center: { lat: number; lon: number };
  onCenter: (center: { lat: number; lon: number }) => void;
  targetSpanM?: number;
  gpsUnavailable?: boolean;
}) {
  const [drawStart, setDrawStart] = useState<{ x: number; y: number } | null>(null);
  const [drawEnd, setDrawEnd] = useState<{ x: number; y: number } | null>(null);
  const [panStart, setPanStart] = useState<{ x: number; y: number; cx: number; cy: number } | null>(null);
  const [spanM, setSpanM] = useState(520);
  const width = 920;
  const height = 620;
  const pad = 0;
  useEffect(() => {
    if (targetSpanM == null) return;
    setSpanM(Math.max(80, Math.min(8000, targetSpanM)));
  }, [targetSpanM]);
  const projectedCenter = project(center.lat, center.lon);
  const bounds = mapBoundsFromCenter(projectedCenter.mx, projectedCenter.my, width, height, spanM, pad);
  const tiles = satelliteTiles(bounds);
  const line = points.length >= 2
    ? points.map((p) => {
      const projected = project(p.lat, p.lon);
      return `${bounds.x(projected.mx)},${bounds.y(projected.my)}`;
    }).join(" ")
    : "";
  const liveProjected = liveSample && liveSample.lat != null && liveSample.lon != null ? project(liveSample.lat, liveSample.lon) : null;
  const pointer = (event: MouseEvent<SVGSVGElement>) => {
    const rect = event.currentTarget.getBoundingClientRect();
    return {
      x: ((event.clientX - rect.left) / rect.width) * width,
      y: ((event.clientY - rect.top) / rect.height) * height,
    };
  };
  const finishDraw = (end: { x: number; y: number }) => {
    if (!drawMode || !drawStart) return;
    if (Math.hypot(end.x - drawStart.x, end.y - drawStart.y) < 8) return;
    const a = bounds.latLon(drawStart.x, drawStart.y);
    const b = bounds.latLon(end.x, end.y);
    onDrawGate({
      id: `${drawMode}-${Date.now()}`,
      label: drawMode === "start_finish" ? "Start Finish" : "Split",
      role: drawMode,
      lat1: a.lat,
      lon1: a.lon,
      lat2: b.lat,
      lon2: b.lon,
    });
  };
  return (
    <svg
      className={drawMode ? "map builderMap drawing" : "map builderMap"}
      viewBox={`0 0 ${width} ${height}`}
      onWheel={(event) => {
        event.preventDefault();
        setSpanM((current) => nextMapSpan(current, event.deltaY));
      }}
      onMouseDown={(event) => {
        const start = pointer(event);
        if (drawMode) {
          setDrawStart(start);
          setDrawEnd(start);
          return;
        }
        setPanStart({ ...start, cx: projectedCenter.mx, cy: projectedCenter.my });
      }}
      onMouseMove={(event) => {
        const current = pointer(event);
        if (drawStart) {
          setDrawEnd(current);
          return;
        }
        if (panStart) {
          const dx = (current.x - panStart.x) / bounds.scale;
          const dy = (current.y - panStart.y) / bounds.scale;
          const next = unproject(panStart.cx - dx, panStart.cy + dy);
          onCenter(next);
        }
      }}
      onMouseUp={(event) => {
        const end = pointer(event);
        finishDraw(end);
        setDrawStart(null);
        setDrawEnd(null);
        setPanStart(null);
      }}
      onMouseLeave={() => {
        setDrawStart(null);
        setDrawEnd(null);
        setPanStart(null);
      }}
    >
      <rect x="0" y="0" width={width} height={height} rx="8" />
      {tiles.map((tile) => (
        <image key={`${tile.z}-${tile.x}-${tile.y}`} href={tile.url} x={bounds.x(tile.left)} y={bounds.y(tile.top)} width={bounds.scale * tile.size} height={bounds.scale * tile.size} preserveAspectRatio="none" />
      ))}
      {line ? <polyline points={line} /> : null}
      {gates.map((gate) => {
        const a = project(gate.lat1, gate.lon1);
        const b = project(gate.lat2, gate.lon2);
        const labelX = bounds.x((a.mx + b.mx) / 2);
        const labelY = bounds.y((a.my + b.my) / 2);
        return (
          <g key={gate.id}>
            <line className={gate.role === "start_finish" ? "startGate" : "splitGate"} x1={bounds.x(a.mx)} y1={bounds.y(a.my)} x2={bounds.x(b.mx)} y2={bounds.y(b.my)} />
            <text className={gate.role === "start_finish" ? "gateLabel startGateLabel" : "gateLabel splitGateLabel"} x={labelX + 6} y={labelY - 6}>{gate.label}</text>
          </g>
        );
      })}
      {liveProjected ? (
        <circle className="liveDot" cx={bounds.x(liveProjected.mx)} cy={bounds.y(liveProjected.my)} r="8" />
      ) : null}
      {drawStart && drawEnd ? (
        <line
          className={drawMode === "start_finish" ? "startGate drawingGate" : "splitGate drawingGate"}
          x1={drawStart.x}
          y1={drawStart.y}
          x2={drawEnd.x}
          y2={drawEnd.y}
        />
      ) : null}
      {gpsUnavailable ? (
        <g className="mapGpsBadge">
          <rect x={width / 2 - 170} y="16" width="340" height="38" rx="8" />
          <text x={width / 2} y="40" textAnchor="middle">Waiting for GPS fix — no position data yet</text>
        </g>
      ) : null}
      <text className="mapCredit" x={width - 10} y={height - 10} textAnchor="end">Esri World Imagery</text>
    </svg>
  );
}

function LapPreview({ rows, hasStartFinish, gpsPointCount }: { rows: LapPreviewRow[]; hasStartFinish: boolean; gpsPointCount: number }) {
  let emptyMessage = "No start/finish crossings detected in the selected GPS trace.";
  if (!gpsPointCount) emptyMessage = "No GPS samples loaded for the selected preview.";
  else if (!hasStartFinish) emptyMessage = "Draw a start/finish gate to preview generated laps.";
  return (
    <div className="lapPreview">
      <div className="lapPreviewHeader">
        <strong>Generated Laps</strong>
        <span>{rows.length ? `${rows.length} rows` : "No lap rows"}</span>
      </div>
      {rows.length ? (
        <table className="lapTable">
          <thead>
            <tr>
              <th>Lap</th>
              <th>Type</th>
              <th>Time</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((row) => (
              <tr key={row.id}>
                <td>{row.label}</td>
                <td>{row.kind === "outlap" ? "Outlap" : "Timed"}</td>
                <td>{formatLapTime(row.durationMs)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      ) : (
        <small className="muted">{emptyMessage}</small>
      )}
    </div>
  );
}

function project(lat: number, lon: number) {
  const radius = 6378137;
  const clamped = Math.max(-85.0511, Math.min(85.0511, lat));
  const mx = radius * lon * Math.PI / 180;
  const my = radius * Math.log(Math.tan(Math.PI / 4 + (clamped * Math.PI) / 360));
  return { mx, my };
}

function unproject(mx: number, my: number) {
  const radius = 6378137;
  const lon = (mx / radius) * 180 / Math.PI;
  const lat = (2 * Math.atan(Math.exp(my / radius)) - Math.PI / 2) * 180 / Math.PI;
  return { lat, lon };
}

function mapBounds(points: Array<{ mx: number; my: number }>, width: number, height: number, pad: number) {
  const xs = points.map((p) => p.mx);
  const ys = points.map((p) => p.my);
  const minX = xs.length ? Math.min(...xs) : -10879330;
  const maxX = xs.length ? Math.max(...xs) : -10879030;
  const minY = ys.length ? Math.min(...ys) : 3547800;
  const maxY = ys.length ? Math.max(...ys) : 3548100;
  const span = Math.max(maxX - minX, maxY - minY, 80);
  const cx = (minX + maxX) / 2;
  const cy = (minY + maxY) / 2;
  const scale = Math.min((width - pad * 2) / span, (height - pad * 2) / span);
  const viewMinX = cx - (width - pad * 2) / scale / 2;
  const viewMaxY = cy + (height - pad * 2) / scale / 2;
  return {
    minX: viewMinX,
    maxX: cx + (width - pad * 2) / scale / 2,
    minY: cy - (height - pad * 2) / scale / 2,
    maxY: viewMaxY,
    scale,
    x: (mx: number) => pad + (mx - viewMinX) * scale,
    y: (my: number) => pad + (viewMaxY - my) * scale,
    latLon: (x: number, y: number) => unproject(viewMinX + (x - pad) / scale, viewMaxY - (y - pad) / scale),
  };
}

function mapBoundsFromCenter(centerX: number, centerY: number, width: number, height: number, spanM: number, pad: number) {
  const scale = Math.min((width - pad * 2) / spanM, (height - pad * 2) / spanM);
  const viewWidth = (width - pad * 2) / scale;
  const viewHeight = (height - pad * 2) / scale;
  const minX = centerX - viewWidth / 2;
  const maxY = centerY + viewHeight / 2;
  return {
    minX,
    maxX: centerX + viewWidth / 2,
    minY: centerY - viewHeight / 2,
    maxY,
    scale,
    x: (mx: number) => pad + (mx - minX) * scale,
    y: (my: number) => pad + (maxY - my) * scale,
    latLon: (x: number, y: number) => unproject(minX + (x - pad) / scale, maxY - (y - pad) / scale),
  };
}

function satelliteTiles(bounds: ReturnType<typeof mapBounds>) {
  const z = 18;
  const origin = 20037508.342789244;
  const tileSize = (origin * 2) / 2 ** z;
  const minTileX = Math.floor((bounds.minX + origin) / tileSize);
  const maxTileX = Math.floor((bounds.maxX + origin) / tileSize);
  const minTileY = Math.floor((origin - bounds.maxY) / tileSize);
  const maxTileY = Math.floor((origin - bounds.minY) / tileSize);
  const tiles: Array<{ z: number; x: number; y: number; left: number; top: number; size: number; url: string }> = [];
  for (let x = minTileX; x <= maxTileX; x += 1) {
    for (let y = minTileY; y <= maxTileY; y += 1) {
      tiles.push({
        z,
        x,
        y,
        left: x * tileSize - origin,
        top: origin - y * tileSize,
        size: tileSize,
        url: `https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/${z}/${y}/${x}`,
      });
    }
  }
  return tiles.slice(0, 64);
}

function defaultMetadataBase(sourceLabel: string, selectedDate: string): SessionMetadata {
  return {
    ...EMPTY_METADATA,
    vehicle_id: sourceLabel,
    event: `${sourceLabel} Telemetry Export`,
    session: selectedDate,
  };
}

function defaultMetadataForSegment(segment: SegmentSummary, sourceLabel: string, selectedDate: string): SessionMetadata {
  return {
    ...defaultMetadataBase(sourceLabel, selectedDate),
    session: segment.label,
    short_comment: segment.id,
  };
}

function summarizeSegments(segments: SegmentSummary[]) {
  if (!segments.length) return null;
  const startMs = Math.min(...segments.map((segment) => segment.start_ms));
  const endMs = Math.max(...segments.map((segment) => segment.end_ms));
  const durationS = segments.reduce((total, segment) => total + segment.duration_s, 0);
  return { startMs, endMs, durationS };
}

function trackViewFromGates(gates: GateLine[]) {
  if (!gates.length) return null;
  const projected = gates.flatMap((gate) => [project(gate.lat1, gate.lon1), project(gate.lat2, gate.lon2)]);
  const xs = projected.map((point) => point.mx);
  const ys = projected.map((point) => point.my);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);
  const center = unproject((minX + maxX) / 2, (minY + maxY) / 2);
  const spanM = Math.max(160, Math.min(8000, Math.max(maxX - minX, maxY - minY) * 1.8));
  return { center, spanM };
}

function normalizeTrack(track: TrackDefinition): TrackDefinition {
  let hasStartFinish = false;
  const gates = track.gates.filter((gate) => {
    if (gate.role !== "start_finish") return true;
    if (hasStartFinish) return false;
    hasStartFinish = true;
    return true;
  });
  return {
    ...track,
    name: track.name || "New Track",
    slug: track.slug || slugifyTrackName(track.name || "New Track"),
    gates: normalizeGateLabels(gates),
  };
}

function normalizeGateLabels(gates: GateLine[]) {
  let splitNumber = 1;
  return gates.map((gate) => {
    if (gate.role === "start_finish") {
      return { ...gate, label: gate.label.trim() || "Start Finish" };
    }
    const trimmed = gate.label.trim();
    const shouldAutoName = !trimmed || /^Split\s+\d+$/i.test(trimmed);
    const label = shouldAutoName ? `Split ${splitNumber}` : trimmed;
    splitNumber += 1;
    return { ...gate, label };
  });
}

function buildLapPreview(points: GpsPoint[], gates: GateLine[], summary: ReturnType<typeof summarizeSegments>): LapPreviewRow[] {
  const startGate = gates.find((gate) => gate.role === "start_finish");
  if (!startGate || points.length < 2) return [];
  const windowStart = summary?.startMs ?? points[0].t;
  const windowEnd = summary?.endMs ?? points.at(-1)?.t ?? windowStart;
  const crossings = dedupeCrossings(gateCrossingTimes(points, startGate), 1500)
    .filter((time) => time > windowStart && time < windowEnd)
    .sort((a, b) => a - b);
  if (!crossings.length) return [];

  const rows: LapPreviewRow[] = [];
  rows.push({
    id: "outlap-start",
    label: "Out lap",
    kind: "outlap",
    startMs: windowStart,
    endMs: crossings[0],
    durationMs: crossings[0] - windowStart,
  });
  for (let index = 0; index < crossings.length - 1; index += 1) {
    rows.push({
      id: `lap-${index + 1}`,
      label: `Lap ${index + 1}`,
      kind: "lap",
      startMs: crossings[index],
      endMs: crossings[index + 1],
      durationMs: crossings[index + 1] - crossings[index],
    });
  }
  const finalCrossing = crossings.at(-1) ?? windowStart;
  rows.push({
    id: "outlap-end",
    label: "Out lap",
    kind: "outlap",
    startMs: finalCrossing,
    endMs: windowEnd,
    durationMs: windowEnd - finalCrossing,
  });
  return rows.filter((row) => row.durationMs > 0);
}

function gateCrossingTimes(points: GpsPoint[], gate: GateLine) {
  const crossings: number[] = [];
  for (let index = 1; index < points.length; index += 1) {
    const previous = points[index - 1];
    const current = points[index];
    if (
      segmentsCross(
        [previous.lon, previous.lat],
        [current.lon, current.lat],
        [gate.lon1, gate.lat1],
        [gate.lon2, gate.lat2],
      )
    ) {
      crossings.push(current.t);
    }
  }
  return crossings;
}

function segmentsCross(a: [number, number], b: [number, number], c: [number, number], d: [number, number]) {
  const o1 = orientation(a, b, c);
  const o2 = orientation(a, b, d);
  const o3 = orientation(c, d, a);
  const o4 = orientation(c, d, b);
  return o1 * o2 < 0 && o3 * o4 < 0;
}

function orientation(a: [number, number], b: [number, number], c: [number, number]) {
  return (b[1] - a[1]) * (c[0] - b[0]) - (b[0] - a[0]) * (c[1] - b[1]);
}

function dedupeCrossings(times: number[], minimumGapMs: number) {
  const deduped: number[] = [];
  times.forEach((time) => {
    if (!deduped.length || time - deduped[deduped.length - 1] >= minimumGapMs) {
      deduped.push(time);
    }
  });
  return deduped;
}

function hasGps(sample: LiveSample): sample is LiveSample & { lat: number; lon: number } {
  return typeof sample.lat === "number" && typeof sample.lon === "number"
    && Number.isFinite(sample.lat) && Number.isFinite(sample.lon)
    // Reject the (0,0) null-island sentinel a module emits before it has a fix.
    && !(sample.lat === 0 && sample.lon === 0);
}

// A single GPS step is implausible above this ground speed (~270 km/h) — used to
// reject teleport glitches that would otherwise fire a false gate crossing.
const MAX_PLAUSIBLE_GPS_MPS = 75;

function distanceMeters(lat1: number, lon1: number, lat2: number, lon2: number) {
  const radius = 6371000;
  const phi1 = (lat1 * Math.PI) / 180;
  const phi2 = (lat2 * Math.PI) / 180;
  const dPhi = ((lat2 - lat1) * Math.PI) / 180;
  const dLambda = ((lon2 - lon1) * Math.PI) / 180;
  const a = Math.sin(dPhi / 2) ** 2 + Math.cos(phi1) * Math.cos(phi2) * Math.sin(dLambda / 2) ** 2;
  return radius * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function nextMapSpan(currentSpanM: number, deltaY: number) {
  const boundedDelta = Math.max(-120, Math.min(120, deltaY));
  const zoomFactor = Math.exp(boundedDelta * 0.0007);
  return Math.max(80, Math.min(8000, currentSpanM * zoomFactor));
}

function sampleCrossesGate(previous: LiveSample, current: LiveSample, gate: GateLine) {
  if (!hasGps(previous) || !hasGps(current)) return false;
  return segmentsCross(
    [previous.lon, previous.lat],
    [current.lon, current.lat],
    [gate.lon1, gate.lat1],
    [gate.lon2, gate.lat2],
  );
}

function bestSectorTimes(laps: LiveLap[]) {
  const sectorCount = Math.max(0, ...laps.map((lap) => lap.sectors.length));
  return Array.from({ length: sectorCount }, (_unused, index) => {
    const values = laps.map((lap) => lap.sectors[index]).filter((value): value is number => typeof value === "number" && value > 0);
    return values.length ? Math.min(...values) : null;
  });
}

function estimateDeltaRate(sample: LiveSample, bestLap: LiveLap | null) {
  if (!bestLap || sample.speed == null || sample.speed <= 0) return null;
  const bestAverageSpeed = bestLap.avgSpeedMps ?? 20;
  return Math.max(-1.5, Math.min(1.5, bestAverageSpeed / Math.max(0.5, sample.speed) - 1));
}

function parseLatLon(value: string) {
  const parts = value.split(/[,\s]+/).map((part) => Number(part.trim())).filter((part) => Number.isFinite(part));
  if (parts.length < 2) return null;
  const [lat, lon] = parts;
  if (Math.abs(lat) > 90 || Math.abs(lon) > 180) return null;
  return { lat, lon };
}

function slugifyTrackName(value: string) {
  return value.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/(^-|-$)/g, "") || "track";
}

// Filesystem-safe label for download filenames (preserves case, collapses junk).
function safeFileLabel(value: string) {
  return value.trim().replace(/[^a-zA-Z0-9._-]+/g, "_").replace(/(^_|_$)/g, "") || "recording";
}

function defaultChannelChartSlugForSource(source: CarPreset["source"]) {
  return source === "angelique" ? ANGELIQUE_CHANNEL_CHART_SLUG : DEFAULT_CHANNEL_CHART.slug;
}

function preferredChannelChart(charts: ChannelChartDefinition[], source: CarPreset["source"]) {
  const defaultSlug = defaultChannelChartSlugForSource(source);
  const sourceToken = source.toLowerCase();
  return (
    charts.find((chart) => chart.slug === defaultSlug) ??
    charts.find((chart) => chart.slug.toLowerCase().includes(sourceToken) || chart.name.toLowerCase().includes(sourceToken)) ??
    charts[0] ??
    DEFAULT_CHANNEL_CHART
  );
}

function parseChannelChartJson(text: string, fileName: string): ChannelChartDefinition {
  const parsed = JSON.parse(text);
  if (Array.isArray(parsed)) {
    return {
      name: fileName.replace(/\.[^.]+$/, ""),
      slug: slugifyTrackName(fileName.replace(/\.[^.]+$/, "")),
      notes: "Imported channel chart",
      entries: parsed.map(normalizeChannelChartEntry),
    };
  }
  if (!parsed || !Array.isArray(parsed.entries)) throw new Error("Channel chart JSON must contain an entries array.");
  return {
    ...DEFAULT_CHANNEL_CHART,
    ...parsed,
    name: parsed.name || fileName.replace(/\.[^.]+$/, ""),
    slug: parsed.slug || slugifyTrackName(parsed.name || fileName.replace(/\.[^.]+$/, "")),
    entries: parsed.entries.map(normalizeChannelChartEntry),
  };
}

function parseChannelChartCsv(text: string, fileName: string): ChannelChartDefinition {
  const rows = parseCsv(text);
  if (!rows.length) throw new Error("Channel chart CSV is empty.");
  const headers = rows[0].map((header) => header.trim().toLowerCase());
  const channelIndex = findHeader(headers, ["channel_name", "channel", "name"]);
  const quantityIndex = findHeader(headers, ["quantity_type", "quantity", "qty", "type"]);
  const unitIndex = findHeader(headers, ["unit", "units"]);
  const notesIndex = findHeader(headers, ["notes", "note"]);
  if (channelIndex < 0 || (quantityIndex < 0 && unitIndex < 0)) {
    throw new Error("Channel chart CSV must contain channel_name plus quantity_type and/or unit columns.");
  }
  const entries = rows.slice(1)
    .map((row) => ({
      channel_name: row[channelIndex]?.trim() ?? "",
      quantity_type: quantityIndex >= 0 ? row[quantityIndex]?.trim() ?? "" : "",
      unit: unitIndex >= 0 ? row[unitIndex]?.trim() ?? "" : "",
      notes: notesIndex >= 0 ? row[notesIndex]?.trim() ?? "" : "",
    }))
    .filter((entry) => entry.channel_name && !entry.channel_name.startsWith("#"));
  return {
    name: fileName.replace(/\.[^.]+$/, ""),
    slug: slugifyTrackName(fileName.replace(/\.[^.]+$/, "")),
    notes: "Imported from CSV channel chart.",
    entries,
  };
}

function normalizeChannelChartEntry(entry: unknown) {
  const value = entry as Partial<{ channel_name: string; channel: string; name: string; quantity_type: string; quantity: string; unit: string; units: string; notes: string }>;
  return {
    channel_name: String(value.channel_name ?? value.channel ?? value.name ?? ""),
    quantity_type: String(value.quantity_type ?? value.quantity ?? ""),
    unit: String(value.unit ?? value.units ?? ""),
    notes: String(value.notes ?? ""),
  };
}

function findHeader(headers: string[], names: string[]) {
  return headers.findIndex((header) => names.includes(header));
}

function parseCsv(text: string): string[][] {
  const rows: string[][] = [];
  let row: string[] = [];
  let field = "";
  let quoted = false;
  for (let index = 0; index < text.length; index += 1) {
    const char = text[index];
    const next = text[index + 1];
    if (quoted) {
      if (char === "\"" && next === "\"") {
        field += "\"";
        index += 1;
      } else if (char === "\"") {
        quoted = false;
      } else {
        field += char;
      }
      continue;
    }
    if (char === "\"") {
      quoted = true;
    } else if (char === ",") {
      row.push(field);
      field = "";
    } else if (char === "\n") {
      row.push(field);
      rows.push(row);
      row = [];
      field = "";
    } else if (char !== "\r") {
      field += char;
    }
  }
  row.push(field);
  rows.push(row);
  return rows.filter((item) => item.some((value) => value.trim()));
}

function formatTime(ms: number) {
  return new Date(ms).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
}

function formatDateTime(ms: number) {
  return new Date(ms).toLocaleString([], { dateStyle: "medium", timeStyle: "medium" });
}

function formatDuration(seconds: number) {
  if (seconds > 3600) return `${(seconds / 3600).toFixed(1)} hr`;
  if (seconds > 60) return `${(seconds / 60).toFixed(1)} min`;
  return `${seconds.toFixed(1)} s`;
}

// Map trackside car/source + event-type strings to the drive_day lookup-table
// ids the Driveday DB uses (lut_car / lut_event_type), so a trackside session
// can create its drive_day record.
function carIdForSource(car: string): number | undefined {
  const c = (car || "").toLowerCase();
  if (c === "orion") return 5;
  if (c === "angelique") return 3;
  return undefined;
}
function eventTypeIdFor(eventType: string): number {
  switch ((eventType || "").toLowerCase()) {
    case "endurance": return 1;
    case "autocross": return 2;
    case "skidpad": return 3;
    case "acceleration": case "straight line acceleration": return 4;
    case "braking": case "straight line braking": return 5;
    default: return 0; // Practice / Efficiency / Test / Other
  }
}

function formatLapTime(ms: number) {
  const totalSeconds = Math.max(0, ms / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return `${minutes}:${seconds.toFixed(2).padStart(5, "0")}`;
}

// Parse an edited lap time. Accepts "M:SS.ss" / "MM:SS.ss" or plain seconds
// ("83.45"). Returns milliseconds, or null if it can't be parsed.
function parseLapTimeInput(text: string): number | null {
  const s = text.trim();
  if (!s) return null;
  const colon = s.match(/^(\d+):(\d{1,2}(?:\.\d+)?)$/);
  if (colon) {
    const min = Number(colon[1]);
    const sec = Number(colon[2]);
    if (!Number.isFinite(min) || !Number.isFinite(sec) || sec >= 60) return null;
    return Math.round((min * 60 + sec) * 1000);
  }
  const n = Number(s);
  return Number.isFinite(n) && n >= 0 ? Math.round(n * 1000) : null;
}

// Lap-time cell: click to edit (M:SS.ss or seconds), Enter/blur commits, Esc
// cancels. Read-only span when not editable (mirror clients).
function EditableLapTime({ durationMs, className, editable, onCommit }: {
  durationMs: number; className?: string; editable: boolean; onCommit: (ms: number) => void;
}) {
  const [editing, setEditing] = useState(false);
  const [text, setText] = useState("");
  if (!editable) return <span className={className}>{formatLapTime(durationMs)}</span>;
  if (editing) {
    const commit = () => {
      const ms = parseLapTimeInput(text);
      setEditing(false);
      if (ms != null && Math.abs(ms - durationMs) >= 1) onCommit(ms);
    };
    return (
      <input
        autoFocus
        value={text}
        onChange={(e) => setText(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => { if (e.key === "Enter") (e.target as HTMLInputElement).blur(); else if (e.key === "Escape") setEditing(false); }}
        aria-label="Edit lap time (M:SS.ss or seconds)"
        style={{ width: 78, padding: "1px 4px", font: "inherit", background: "var(--surface-alt)", border: "1px solid var(--accent)", borderRadius: 4, color: "var(--text)" }}
      />
    );
  }
  return (
    <button
      className={className}
      title="Click to edit lap time (M:SS.ss or seconds)"
      onClick={() => { setText(formatLapTime(durationMs)); setEditing(true); }}
      style={{ background: "none", border: "none", color: "inherit", font: "inherit", cursor: "text", padding: 0, borderBottom: "1px dotted var(--muted-text)" }}
    >
      {formatLapTime(durationMs)}
    </button>
  );
}

function formatSignedSeconds(ms: number) {
  const seconds = ms / 1000;
  return `${seconds >= 0 ? "+" : ""}${seconds.toFixed(2)} s`;
}

function formatSpeed(speed: number) {
  return `${(speed * 2.23694).toFixed(1)} mph`;
}

// Canonicalize a live-value key for matching: lowercase + drop every
// non-alphanumeric char. This makes lookups robust to the different shapes the
// same signal arrives as — snake (`controls_bse1_v`), camel from the protobuf
// decoder (`controls_bse1V`), or dotted (`controls.bse1_v`) — all collapse to
// `controlsbse1v`. Without this, decoded sensor_data (camelCased) silently
// missed the panel's snake_case lookups, blanking steering/brake/accel.
function normalizeLiveKey(key: string): string {
  return key.toLowerCase().replace(/[^a-z0-9]/g, "");
}

const liveValueIndexCache = new WeakMap<Record<string, number>, Map<string, number>>();
function liveValueIndex(values: Record<string, number>): Map<string, number> {
  const cached = liveValueIndexCache.get(values);
  if (cached) return cached;
  const index = new Map<string, number>();
  for (const [key, value] of Object.entries(values)) {
    if (typeof value === "number" && Number.isFinite(value)) {
      const norm = normalizeLiveKey(key);
      if (!index.has(norm)) index.set(norm, value);
    }
  }
  liveValueIndexCache.set(values, index);
  return index;
}

function firstLiveValue(values: Record<string, number>, keys: string[]) {
  // Fast path: exact key hit.
  for (const key of keys) {
    const value = values[key];
    if (typeof value === "number" && Number.isFinite(value)) return value;
  }
  // Fallback: normalized (case/separator-insensitive) match.
  const index = liveValueIndex(values);
  for (const key of keys) {
    const value = index.get(normalizeLiveKey(key));
    if (value !== undefined) return value;
  }
  return null;
}

// Like firstLiveValue but skips channels whose value fails `valid` — so a
// flatlined placeholder (e.g. hv_pack_v stuck at 0 while dc_bus_v carries the
// real voltage) doesn't shadow the channel that's actually reporting. Plain
// firstLiveValue returns the first *finite* hit and then a validity check on
// only that hit nulls the whole thing, which is exactly how the energy/voltage
// readouts were landing on 0.
function firstValidLiveValue(
  values: Record<string, number>,
  keys: string[],
  valid: (v: number) => boolean,
): number | null {
  for (const key of keys) {
    const value = values[key];
    if (typeof value === "number" && Number.isFinite(value) && valid(value)) return value;
  }
  const index = liveValueIndex(values);
  for (const key of keys) {
    const value = index.get(normalizeLiveKey(key));
    if (value !== undefined && valid(value)) return value;
  }
  return null;
}

// Pick a live bus/pack current. Prefer a channel that's actually carrying
// signal — some feeds (e.g. Orion right now) sit hv_c at a flatline 0 while
// dc_bus_current carries the real current, and listing hv_c first used to win
// and zero out the DC-bus power. A flatlined 0 only wins if EVERY current
// channel reads 0 (a genuine standstill), in which case 0 is correct.
function liveCurrentFor(values: Record<string, number>, keys: string[]): number | null {
  let sawValidZero = false;
  const consider = (v: number | undefined): number | undefined => {
    if (typeof v !== "number" || !Number.isFinite(v) || !validDcBusCurrent(v)) return undefined;
    if (v !== 0) return v;
    sawValidZero = true;
    return undefined;
  };
  for (const key of keys) {
    const r = consider(values[key]);
    if (r !== undefined) return r;
  }
  const index = liveValueIndex(values);
  for (const key of keys) {
    const r = consider(index.get(normalizeLiveKey(key)));
    if (r !== undefined) return r;
  }
  return sawValidZero ? 0 : null;
}

function smoothLiveSamples(samples: LiveSample[]) {
  let previous: LiveSample | null = null;
  return samples.map((sample) => {
    const smoothed = smoothLiveSample(sample, previous);
    previous = smoothed;
    return smoothed;
  });
}

function smoothLiveSample(sample: LiveSample, previous: LiveSample | null) {
    const dtSeconds = previous ? Math.max(0.02, (sample.t - previous.t) / 1000) : 0.1;
    const values = { ...sample.values };
    const smoothed: LiveSample = { ...sample, values };

    const voltage = measuredPackVoltageFor(sample);
    const current = packCurrentFor(sample);
    const speed = sample.speed ?? sample.values.speed ?? null;

    const previousVoltage = previous ? packVoltageFor(previous) : null;
    const previousCurrent = previous ? packCurrentFor(previous) : null;
    const previousSpeed = previous?.speed ?? previous?.values.speed ?? null;

    const filteredVoltage = smoothPresentNumber(voltage, previousVoltage, dtSeconds, 3.5);
    const filteredCurrent = smoothPresentNumber(current, previousCurrent, dtSeconds, 1.1);
    const filteredSpeed = smoothPresentNumber(speed, previousSpeed, dtSeconds, 0.8);

    if (filteredVoltage != null) {
      smoothed.hv_pack_v = filteredVoltage;
      values.hv_pack_v = filteredVoltage;
      values.dc_bus_v = filteredVoltage;
    }
    if (filteredCurrent != null) {
      smoothed.hv_c = filteredCurrent;
      values.hv_c = filteredCurrent;
      values.dc_bus_current = filteredCurrent;
    }
    if (filteredSpeed != null) {
      smoothed.speed = filteredSpeed;
      values.speed = filteredSpeed;
    }

    if (hasGps(sample) && isReasonableGps(sample.lat, sample.lon)) {
      const previousGps = previous && hasGps(previous) ? previous : null;
      if (!previousGps) {
        smoothed.lat = sample.lat;
        smoothed.lon = sample.lon;
      } else {
        const jumpSpeedMps = distanceMeters(previousGps.lat, previousGps.lon, sample.lat, sample.lon) / dtSeconds;
        if (jumpSpeedMps > 85) {
          smoothed.lat = previousGps.lat;
          smoothed.lon = previousGps.lon;
        } else {
          const alpha = alphaForDt(dtSeconds, 0.65);
          smoothed.lat = previousGps.lat + (sample.lat - previousGps.lat) * alpha;
          smoothed.lon = previousGps.lon + (sample.lon - previousGps.lon) * alpha;
        }
      }
    } else {
      smoothed.lat = null;
      smoothed.lon = null;
    }

    return smoothed;
}

function isReasonableGps(lat: number, lon: number) {
  return Number.isFinite(lat) && Number.isFinite(lon) && Math.abs(lat) <= 90 && Math.abs(lon) <= 180 && Math.hypot(lat, lon) > 0.001;
}

function smoothNumber(value: number | null, previous: number | null, dtSeconds: number, tauSeconds: number) {
  if (value == null || !Number.isFinite(value)) return previous;
  if (previous == null || !Number.isFinite(previous)) return value;
  const alpha = alphaForDt(dtSeconds, tauSeconds);
  return previous + (value - previous) * alpha;
}

function smoothPresentNumber(value: number | null, previous: number | null, dtSeconds: number, tauSeconds: number) {
  if (value == null || !Number.isFinite(value)) return null;
  return smoothNumber(value, previous, dtSeconds, tauSeconds);
}

function alphaForDt(dtSeconds: number, tauSeconds: number) {
  return clamp(1 - Math.exp(-Math.max(0.001, dtSeconds) / Math.max(0.001, tauSeconds)), 0.02, 0.45);
}

// Accumulator geometry: 130 series cells (HVC hvc_bms.c TOTAL_IC*CELLS_PER_IC)
// x 5 parallel Molicel P30B cells (3.0 Ah, 3.6 V nominal => 10.8 Wh/cell).
const PACK_SERIES_CELLS = 130;
const PACK_PARALLEL_CELLS = 5;
const CELL_CAPACITY_AH = 3.0;
const CELL_NOMINAL_V = 3.6;
const PACK_ENERGY_KWH = (PACK_SERIES_CELLS * PACK_PARALLEL_CELLS * CELL_CAPACITY_AH * CELL_NOMINAL_V) / 1000;
const DEFAULT_SOE_CUTOFF_CELL_V = 2.8;
const DEFAULT_SOE_DERATE_START_CELL_V = 3.2;
const OCV_UPDATE_CURRENT_THRESHOLD_A = 1.0;
const OCV_LPF_TIME_CONSTANT_S = 20.0;
const OCV_INITIAL_SAMPLE_COUNT = 4;

function loadSoeCutoffCellV() {
  const raw = localStorage.getItem("motec-soe-cutoff-cell-v");
  if (!raw) return DEFAULT_SOE_CUTOFF_CELL_V;
  const parsed = Number(raw);
  return isLegacyDefaultSoeCutoff(parsed) ? DEFAULT_SOE_CUTOFF_CELL_V : normalizeSoeCutoffCellV(parsed);
}

function normalizeSavedSoeCutoffCellV(value: number | null | undefined) {
  return isLegacyDefaultSoeCutoff(value) ? DEFAULT_SOE_CUTOFF_CELL_V : normalizeSoeCutoffCellV(value);
}

function normalizeSoeCutoffCellV(value: number | null | undefined) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric <= 0) return DEFAULT_SOE_CUTOFF_CELL_V;
  return clamp(numeric, 2, 4.2);
}

function isLegacyDefaultSoeCutoff(value: number | null | undefined) {
  return typeof value === "number" && (Math.abs(value - 2.5) < 0.001 || Math.abs(value - 3.0) < 0.001);
}

function estimateSoeCellVoltage(samples: LiveSample[]) {
  let estimate: number | null = null;
  let previousT: number | null = null;
  const initialSamples: number[] = [];
  for (const sample of samples) {
    const minCell = minCellVoltageFor(sample);
    const ocvCurrent = ocvRestCurrentFor(sample);
    const isOcvSample = validCellVoltage(minCell) && ocvCurrent != null && Math.abs(ocvCurrent) < OCV_UPDATE_CURRENT_THRESHOLD_A;
    if (!isOcvSample) {
      previousT = sample.t;
      continue;
    }
    if (estimate == null) {
      initialSamples.push(minCell);
      previousT = sample.t;
      if (initialSamples.length >= OCV_INITIAL_SAMPLE_COUNT) {
        estimate = medianNumber(initialSamples);
      }
      continue;
    }
    const dtSeconds = previousT == null ? 0 : Math.max(0, (sample.t - previousT) / 1000);
    estimate = lpfStepFirmware(estimate, minCell, lpfAlphaFromTau(dtSeconds, OCV_LPF_TIME_CONSTANT_S));
    previousT = sample.t;
  }
  return estimate;
}

function medianNumber(values: number[]) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.floor(sorted.length / 2)] ?? null;
}

function ocvRestCurrentFor(sample: LiveSample | null) {
  const dcBusCurrent = dcBusCurrentFor(sample);
  if (dcBusCurrent != null) return dcBusCurrent;
  if (!sample) return null;
  const values = sample.values ?? {};
  const rpm = motorSpeedRpmFor(sample);
  const speed = sample.speed ?? firstLiveValue(values, ["speed", "gps_speed", "dynamics_gps_speed"]);
  const stationary = (rpm == null || Math.abs(rpm) < 50) && (speed == null || Math.abs(speed) < 1);
  if (!stationary) return null;
  const phaseCurrents = [
    firstLiveValue(values, ["phase_a_current", "pack_phase_a_current", "inverter_phase_a_current"]),
    firstLiveValue(values, ["phase_b_current", "pack_phase_b_current", "inverter_phase_b_current"]),
    firstLiveValue(values, ["phase_c_current", "pack_phase_c_current", "inverter_phase_c_current"]),
  ].filter((value): value is number => value != null && Number.isFinite(value));
  if (!phaseCurrents.length) return null;
  const maxAbsPhaseCurrent = Math.max(...phaseCurrents.map((value) => Math.abs(value)));
  return maxAbsPhaseCurrent;
}

function lpfAlphaFromTau(dtSeconds: number, tauSeconds: number) {
  if (tauSeconds <= 0) return 1;
  return clamp(dtSeconds / (tauSeconds + dtSeconds), 0, 1);
}

function lpfStepFirmware(previous: number, value: number, alpha: number) {
  return alpha * value + (1 - alpha) * previous;
}

function lowVoltageDerateFromOcv(cellVoltage: number, startCellV: number, cutoffCellV: number) {
  if (startCellV <= cutoffCellV) return cellVoltage > cutoffCellV ? 1 : 0;
  return clamp((cellVoltage - cutoffCellV) / (startCellV - cutoffCellV), 0, 1);
}

function packStatus(samples: LiveSample[], soeCutoffCellV: number, displaySample?: LiveSample | null) {
  const sample = displaySample ?? samples.at(-1) ?? null;
  const voltage = packVoltageFor(sample);
  const voltageSource = packVoltageSourceFor(sample);
  const dcBusV = dcBusVoltageFor(sample);
  const minCellV = minCellVoltageFor(sample);
  const soeCellV = estimateSoeCellVoltage(samples);
  // SOE Est reads ONLY the car's reported SOC (VCU soc_estimate / hv_soc, the
  // same value the car dash shows). The min-cell OCV fallback was sagging under
  // load and disagreed with the dash — by user request we no longer derive an
  // SOC from OCV. If the car isn't reporting SOC, the gauge shows "--" and the
  // caption explains why. (soeCellV is still computed because it drives the
  // separate OCV-based safety derate; that lives below.)
  const soePercent = carSoePercentFor(sample);
  const soeSource = soePercent != null ? "car" : "missing";
  const soeKwh = soePercent != null ? (soePercent / 100) * PACK_ENERGY_KWH : null;
  const lowVoltageDeratePct =
    soeCellV != null ? lowVoltageDerateFromOcv(soeCellV, DEFAULT_SOE_DERATE_START_CELL_V, soeCutoffCellV) * 100 : null;
  const rawMaxCellV = maxCellVoltageFor(sample);
  const maxCellV = rawMaxCellV != null && minCellV != null && rawMaxCellV < minCellV ? minCellV : rawMaxCellV ?? minCellV;
  const maxCellTemp = maxCellTempFor(sample);
  return {
    voltage,
    voltageSource,
    dcBusV,
    soeCellV,
    soePercent,
    soeSource,
    soeKwh,
    lowVoltageDeratePct,
    minCellV,
    maxCellV,
    maxCellTemp,
  };
}

function minCellVoltageFor(sample: LiveSample | null) {
  if (!sample) return null;
  const direct = firstLiveValue(sample.values, ["min_cell_voltage", "thermal_min_cell_voltage", "pack_min_cell_voltage", "min_cell_v", "pack_min_cell_v"]);
  if (validCellVoltage(direct)) return direct;
  const cellVoltages = Object.entries(sample.values)
    .filter(([key]) => key.includes("cells_v") || key.includes("cell_voltage"))
    .map(([_key, value]) => value)
    .filter((value): value is number => validCellVoltage(value));
  return cellVoltages.length ? Math.min(...cellVoltages) : null;
}

function maxCellVoltageFor(sample: LiveSample | null) {
  if (!sample) return null;
  const direct = firstLiveValue(sample.values, ["max_cell_voltage", "thermal_max_cell_voltage", "pack_max_cell_voltage", "max_cell_v", "pack_max_cell_v"]);
  if (validCellVoltage(direct)) return direct;
  const cellVoltages = Object.entries(sample.values)
    .filter(([key]) => key.includes("cells_v") || key.includes("cell_voltage"))
    .map(([_key, value]) => value)
    .filter((value): value is number => validCellVoltage(value));
  return cellVoltages.length ? Math.max(...cellVoltages) : null;
}

function carSoePercentFor(sample: LiveSample | null) {
  // The car's own SOC: VCU soc_estimate (what the car dash shows), then hv_soc.
  // Both sit at 0 when their CAN packet is down, so skip non-positive
  // placeholders and only trust a real reading. Accepts 0–1 or 0–100 scaling.
  const values = sample?.values ?? {};
  for (const key of ["soc_estimate", "pack_soc_estimate", "hv_soc", "pack_hv_soc"]) {
    const value = values[key];
    if (typeof value === "number" && Number.isFinite(value) && value > 0) {
      return clamp(value > 1 ? value : value * 100, 0, 100);
    }
  }
  return null;
}

function validCellVoltage(value: number | null | undefined): value is number {
  return value != null && Number.isFinite(value) && value >= 1.5 && value <= 4.5;
}

function maxCellTempFor(sample: LiveSample | null) {
  if (!sample) return null;
  const candidates = [
    firstLiveValue(sample.values, ["cell_top_temp", "thermal_cell_top_temp"]),
    firstLiveValue(sample.values, ["cell_bottom_temp", "thermal_cell_bottom_temp"]),
  ].map(validTemp).filter((value): value is number => value != null);
  return candidates.length ? Math.max(...candidates) : null;
}

function packVoltageFor(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  return measuredPackVoltageFor(sample) ?? firstLiveValue(values, ["cell_pack_voltage_est"]) ?? null;
}

function packVoltageSourceFor(sample: LiveSample | null) {
  if (measuredPackVoltageFor(sample) != null) return "measured";
  const values = sample?.values ?? {};
  return firstLiveValue(values, ["cell_pack_voltage_est"]) == null ? "missing" : "cell-est";
}

function measuredPackVoltageFor(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  return firstValidLiveValue(values, ["hv_pack_v", "dc_bus_v", "bus_voltage", "pack_hv_pack_v", "pack_dc_bus_v"], validDcBusVoltage)
    ?? (validDcBusVoltage(sample?.hv_pack_v) ? sample!.hv_pack_v! : null);
}

function dcBusVoltageFor(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  return firstValidLiveValue(values, [
    "dc_bus_v",
    "pack_dc_bus_v",
    "bus_voltage",
    "pack_bus_voltage",
    "inverter_dc_bus_v",
    "inverter_dc_bus_voltage",
    "inverter_dc_bus_voltage_v",
  ], validDcBusVoltage);
}

function dcBusCurrentFor(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  return liveCurrentFor(values, [
    "dc_bus_current",
    "pack_dc_bus_current",
    "inverter_dc_bus_current",
    "inverter_dc_bus_current_a",
    "inverter_dc_bus_c",
    "hv_c",
    "pack_hv_c",
  ]);
}

function validDcBusVoltage(value: number | null | undefined): value is number {
  return value != null && Number.isFinite(value) && value >= 100 && value <= 700;
}

function validDcBusCurrent(value: number | null | undefined): value is number {
  return value != null && Number.isFinite(value) && value >= -600 && value <= 600;
}

// Per-sample energy delta sourced from the VCU's cumulative net_energy /
// regen_energy (CAN 0x1C9 Energy Estimate, units Wh). The website trusts the
// VCU as the source of truth — no client-side power×dt integration anywhere.
//
// `net_energy` is the cumulative drive-minus-regen total, `regen_energy` is the
// cumulative regen returned. So:
//   drive_cumulative = net + regen
//   regen_cumulative = regen
// And the per-sample deltas drop out cleanly from (this - previous).
//
// Returns {0, 0} when either side is missing or the previous value is unknown,
// and clamps to non-negative deltas so a VCU reboot (counters reset to 0)
// doesn't subtract from the session totals.
function vcuEnergyDeltaWh(
  previous: LiveSample | null,
  sample: LiveSample,
): { outDeltaWh: number; inDeltaWh: number } {
  if (!previous) return { outDeltaWh: 0, inDeltaWh: 0 };
  const prevNet = previous.values?.net_energy;
  const prevRegen = previous.values?.regen_energy;
  const curNet = sample.values?.net_energy;
  const curRegen = sample.values?.regen_energy;
  if (typeof prevNet !== "number" || typeof prevRegen !== "number"
    || typeof curNet !== "number" || typeof curRegen !== "number") {
    return { outDeltaWh: 0, inDeltaWh: 0 };
  }
  const driveDelta = (curNet - prevNet) + (curRegen - prevRegen);
  const regenDelta = curRegen - prevRegen;
  return {
    outDeltaWh: Math.max(0, driveDelta),
    inDeltaWh: Math.max(0, regenDelta),
  };
}

// Signed instantaneous kW between two samples, derived from the VCU's
// cumulative energy. Positive = drive (energy leaving the pack), negative =
// regen (energy returning). Needs both samples present and a finite dt; otherwise
// null (the caller falls back to a stale-but-finite display).
function signedVcuPowerKwFor(previous: LiveSample | null, sample: LiveSample | null) {
  if (!previous || !sample) return null;
  const dtSeconds = (sample.t - previous.t) / 1000;
  if (!Number.isFinite(dtSeconds) || dtSeconds <= 0) return null;
  const { outDeltaWh, inDeltaWh } = vcuEnergyDeltaWh(previous, sample);
  if (outDeltaWh === 0 && inDeltaWh === 0) return null;
  // Wh / s = W; *3.6 = kW.
  return (outDeltaWh - inDeltaWh) * 3.6 / dtSeconds;
}

function energyVoltageFor(sample: LiveSample | null) {
  return dcBusVoltageFor(sample) ?? cellPackVoltageEstimateFor(sample);
}

function cellPackVoltageEstimateFor(sample: LiveSample | null) {
  const value = firstLiveValue(sample?.values ?? {}, ["cell_pack_voltage_est"]);
  return validDcBusVoltage(value) ? value : null;
}

function torqueFeedbackNmFor(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  return firstLiveValue(values, [
    "torque_feedback",
    "controls_torque_feedback",
    "inverter_torque_feedback",
    "inverter_feedback_torque",
    "feedback_torque",
    "actual_torque",
    "motor_torque",
    "dynamics_inverter_torque",
    "inverter_torque",
    "commanded_torque",
    "controls_commanded_torque",
    "torque_request",
    "controls_torque_request",
  ]);
}

function packCurrentFor(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  return liveCurrentFor(values, [
    "dc_bus_current",
    "hv_c",
    "pack_dc_bus_current",
    "pack_hv_c",
    "inverter_dc_bus_current",
    "inverter_dc_bus_current_a",
    "inverter_dc_bus_c",
  ]) ?? (validDcBusCurrent(sample?.hv_c) ? sample!.hv_c! : null);
}



function driverControls(sample: LiveSample | null, torqueParamSet: VcuTorqueParamSet) {
  const values = sample?.values ?? {};
  const throttlePercent = throttleTravelPercent(values);
  const torqueRequest = driverTorqueRequestFor(sample, torqueParamSet);
  const bse1V = firstLiveValue(values, ["bse1_v", "controls_bse1_v"]);
  const bse2V = firstLiveValue(values, ["bse2_v", "controls_bse2_v"]);
  const bse3Pressure = firstLiveValue(values, ["brake_pressure_rbll", "controls_brake_pressure_rbll", "bse3_psi", "controls_bse3_psi"]);
  const bse3V = firstLiveValue(values, ["bse3_v", "controls_bse3_v"]);
  return {
    steeringAngleDeg: firstLiveValue(values, ["steer_col_angle", "dynamics_steer_col_angle", "steering_angle"]),
    throttlePercent: throttlePercent == null ? null : clamp(throttlePercent, 0, 100),
    requestedTorqueNm: torqueRequest.value,
    requestedTorqueSource: torqueRequest.source,
    bse1Psi: bse1V == null ? null : bse1Psi(bse1V),
    bse2Psi: bse2V == null ? null : bse2Psi(bse2V),
    bse3Psi: bse3Pressure ?? (bse3V == null ? null : bse3Psi(bse3V)),
  };
}

function driverTorqueRequestFor(sample: LiveSample | null, params: VcuTorqueParamSet) {
  const values = sample?.values ?? {};
  const liveTorque = liveRequestedTorqueNmFor(values);
  const computedTorque = computedDriverTorqueNmFor(sample, params);
  const motorRpm = motorSpeedRpmFor(sample);
  const speed = sample?.speed ?? firstLiveValue(values, ["speed", "gps_speed", "dynamics_gps_speed"]);
  const carMoving = (motorRpm != null && Math.abs(motorRpm) > 50) || (speed != null && Math.abs(speed) > 1);
  if (carMoving && liveTorque != null) {
    return { value: liveTorque, source: "live" };
  }
  if (computedTorque != null) {
    return { value: computedTorque, source: params.label };
  }
  if (liveTorque != null) {
    return { value: liveTorque, source: "live" };
  }
  return { value: null, source: "calc" };
}

function liveRequestedTorqueNmFor(values: Record<string, number>) {
  const value = firstLiveValue(values, [
    "commanded_torque",
    "controls_commanded_torque",
    "torque_command",
    "controls_torque_command",
    "torque_request",
    "controls_torque_request",
  ]);
  return value != null && Math.abs(value) <= 800 ? value : null;
}

function computedDriverTorqueNmFor(sample: LiveSample | null, params: VcuTorqueParamSet) {
  const values = sample?.values ?? {};
  const rawPedalTravel = rawAppsTravel(values);
  if (rawPedalTravel == null) return null;
  const apps = applyAppsDeadzone(rawPedalTravel, params.appsMinTravelDeadzone, params.appsMaxTravelDeadzone);
  const rpm = clamp(Math.abs(motorSpeedRpmFor(sample) ?? 0), 0, VCU_MAX_MOTOR_RPM);
  const availableTorque = lookupEvenlySpaced(params.powerLimitTorque, rpm, 0, VCU_MAX_MOTOR_RPM);
  const pedalRemapped = lookupByBreakpoints(params.pedalMapX, params.pedalMap, apps);
  const pedalTorqueFraction = Math.pow(clamp(pedalRemapped, 0, 1), params.pedalCurveExponent);
  const lowCellV = minCellVoltageFor(sample);
  const cellDerate = validCellVoltage(lowCellV)
    ? lowVoltageDerateFromOcv(lowCellV, params.lowCellDerateStartV, params.lowCellCutoffV)
    : 1;
  return availableTorque * pedalTorqueFraction * cellDerate;
}

function rawAppsTravel(values: Record<string, number>) {
  const details = rawAppsTravelDetails(values);
  if (details != null) return clamp(details.average, 0, 1);
  const apps1 = firstLiveValue(values, ["apps1_travel", "controls_apps1_travel"]);
  const apps2 = firstLiveValue(values, ["apps2_travel", "controls_apps2_travel"]);
  const avgTravel = averageValues([apps1, apps2]);
  if (avgTravel != null) return clamp(avgTravel, 0, 1);
  const pedalTravel = firstLiveValue(values, ["accel_pedal_travel", "dynamics_accel_pedal_travel"]);
  return pedalTravel == null ? null : clamp(pedalTravel, 0, 1);
}

function rawAppsTravelDetails(values: Record<string, number>) {
  const apps1V = sensorVoltage(firstLiveValue(values, ["apps1_v", "controls_apps1_v"]));
  const apps2V = sensorVoltage(firstLiveValue(values, ["apps2_v", "controls_apps2_v"]));
  if (apps1V == null || apps2V == null) return null;
  const apps1Travel = inverseLinearInterp(1.750, 1.520, apps1V);
  const apps2Travel = inverseLinearInterp(0.190, -0.020, apps2V);
  return {
    apps1Travel,
    apps2Travel,
    average: (apps1Travel + apps2Travel) / 2,
  };
}

function applyAppsDeadzone(travel: number, minTravel: number, maxTravel: number) {
  if (travel <= minTravel) return 0;
  if (travel >= maxTravel) return 1;
  return (travel - minTravel) / (maxTravel - minTravel);
}

function inverseLinearInterp(a: number, b: number, value: number) {
  const span = b - a;
  if (Math.abs(span) < 1e-6) return 0;
  return (value - a) / span;
}

function motorSpeedRpmFor(sample: LiveSample | null) {
  return firstLiveValue(sample?.values ?? {}, ["controls_motor_speed", "motor_speed", "dynamics_inverter_rpm", "inverter_rpm"]);
}

function lookupEvenlySpaced(values: number[], input: number, min: number, max: number) {
  if (!values.length) return 0;
  if (values.length === 1 || input <= min) return values[0];
  if (input >= max) return values[values.length - 1];
  const step = (max - min) / (values.length - 1);
  const lowerIndex = clamp(Math.floor((input - min) / step), 0, values.length - 2);
  const lowerX = min + lowerIndex * step;
  return linearInterp(values[lowerIndex], values[lowerIndex + 1], (input - lowerX) / step);
}

function lookupByBreakpoints(xs: number[], ys: number[], input: number) {
  const count = Math.min(xs.length, ys.length);
  if (!count) return 0;
  if (count === 1 || input <= xs[0]) return ys[0];
  if (input >= xs[count - 1]) return ys[count - 1];
  for (let index = 1; index < count; index += 1) {
    if (input <= xs[index]) {
      return linearInterp(ys[index - 1], ys[index], (input - xs[index - 1]) / (xs[index] - xs[index - 1]));
    }
  }
  return ys[count - 1];
}

function linearInterp(start: number, end: number, t: number) {
  return start + (end - start) * clamp(t, 0, 1);
}

function throttleTravelPercent(values: Record<string, number>) {
  // Display pedal position after VCU deadzones, but before pedal/torque maps.
  const rawTravel = rawAppsTravel(values);
  if (rawTravel != null) return applyAppsDeadzone(rawTravel, 0.09, 0.88) * 100;
  const pedalTravel = firstLiveValue(values, ["accel_pedal_travel", "dynamics_accel_pedal_travel"]);
  if (pedalTravel != null && Number.isFinite(pedalTravel)) {
    return clamp(pedalTravel * 100, 0, 100);
  }
  const apps1 = firstLiveValue(values, ["apps1_travel", "controls_apps1_travel"]);
  const apps2 = firstLiveValue(values, ["apps2_travel", "controls_apps2_travel"]);
  const avgTravel = averageValues([apps1, apps2]);
  return avgTravel == null ? null : clamp(avgTravel * 100, 0, 100);
}

function sensorVoltage(value: number | null) {
  if (value == null || !Number.isFinite(value)) return null;
  if (value > 5 && value <= 4095) return value * 3.3 / 4095;
  return value;
}

function bse1Psi(volts: number) {
  return clamp(2000.6452 * volts - 636.8984, 0, 3000);
}

function bse2Psi(volts: number) {
  return clamp(2309.3868 * volts - 735.1852, 0, 3000);
}

function bse3Psi(volts: number) {
  return bse2Psi(volts);
}

function averageValues(values: Array<number | null>) {
  const finite = values.filter((value): value is number => typeof value === "number" && Number.isFinite(value));
  return finite.length ? finite.reduce((sum, value) => sum + value, 0) / finite.length : null;
}

function tempStatus(sample: LiveSample | null) {
  const values = sample?.values ?? {};
  const moduleB = validTemp(firstLiveValue(values, ["module_b_temp", "thermal_module_b_temp"]));
  const moduleC = validTemp(firstLiveValue(values, ["module_c_temp", "thermal_module_c_temp"]));
  return {
    ambient: validTemp(firstLiveValue(values, ["ambient_temp", "thermal_ambient_temp"])),
    coolant: validTemp(firstLiveValue(values, ["coolant_temp", "thermal_coolant_temp"])),
    fanRpm: firstLiveValue(values, ["fan_rpm", "thermal_fan_rpm", "battery_fan_rpm", "thermal_battery_fan_rpm"]),
    motor: validTemp(firstLiveValue(values, ["motor_temp", "thermal_motor_temp"])),
    inverter: averageValues([moduleB, moduleC]) ?? validTemp(firstLiveValue(values, ["inverter_temp", "thermal_inverter_temp"])),
  };
}

function validTemp(value: number | null) {
  if (value == null || !Number.isFinite(value)) return null;
  return value >= -40 && value <= 180 ? value : null;
}

function temperatureSeries(samples: LiveSample[], windowS: number) {
  const lastT = samples.at(-1)?.t;
  const startT = lastT ? lastT - windowS * 1000 : 0;
  const definitions = [
    { key: "ambient", label: "Ambient", color: "#38bdf8" },
    { key: "coolant", label: "Coolant", color: "#22c55e" },
    { key: "motor", label: "Motor", color: "#f97316" },
    { key: "inverter", label: "Inverter", color: "#e879f9" },
  ] as const;
  const windowSamples = lastT ? samples.filter((sample) => sample.t >= startT) : [];
  return definitions.map((definition) => ({
    ...definition,
    segments: windowSamples.reduce<Array<Array<{ t: number; value: number }>>>((segments, sample) => {
        const value = tempStatus(sample)[definition.key];
        if (value == null) {
          if (segments.at(-1)?.length) segments.push([]);
          return segments;
        }
        if (!segments.length) segments.push([]);
        segments[segments.length - 1].push({ t: sample.t, value });
        return segments;
      }, [])
      .filter((segment) => segment.length),
  }));
}

// Speed / Power / Torque over the rolling window. Different units, so each line
// is normalized to its OWN min/max in the window (shape comparison); the legend
// carries the latest absolute value + unit. Mirrors temperatureSeries' segment
// model (gaps where a channel is missing).
function vitalsSeries(samples: LiveSample[], windowS: number) {
  const lastT = samples.at(-1)?.t;
  const startT = lastT ? lastT - windowS * 1000 : 0;
  const windowSamples = lastT ? samples.filter((sample) => sample.t >= startT) : [];
  const definitions = [
    { key: "speed", label: "Speed", unit: "mph", color: "#38bdf8", digits: 0, get: (s: LiveSample) => (s.speed != null && Number.isFinite(s.speed) ? s.speed * 2.23694 : null) },
    { key: "power", label: "Power", unit: "kW", color: "#f5a524", digits: 1, get: (s: LiveSample) => (s.power_kw != null && Number.isFinite(s.power_kw) ? s.power_kw : null) },
    { key: "torque", label: "Torque", unit: "Nm", color: "#a78bfa", digits: 0, get: (s: LiveSample) => { const v = torqueFeedbackNmFor(s); return v != null && Number.isFinite(v) ? v : null; } },
  ] as const;
  return definitions.map((definition) => {
    const segments = windowSamples.reduce<Array<Array<{ t: number; value: number }>>>((segs, sample) => {
        const value = definition.get(sample);
        if (value == null) { if (segs.at(-1)?.length) segs.push([]); return segs; }
        if (!segs.length) segs.push([]);
        segs[segs.length - 1].push({ t: sample.t, value });
        return segs;
      }, [])
      .filter((segment) => segment.length);
    const all = segments.flatMap((segment) => segment.map((point) => point.value));
    return {
      key: definition.key, label: definition.label, unit: definition.unit, color: definition.color, digits: definition.digits,
      segments,
      min: all.length ? Math.min(...all) : 0,
      max: all.length ? Math.max(...all) : 1,
      latest: segments.at(-1)?.at(-1)?.value ?? null,
    };
  });
}

function VitalsWindowChart({ state, windowS }: { state: LiveSessionState; windowS: number }) {
  const series = vitalsSeries(state.samples, windowS);
  const width = 900;
  const height = 160;
  const padLeft = 46;
  const padRight = 18;
  const padTop = 14;
  const padBottom = 28;
  const plotW = width - padLeft - padRight;
  const plotH = height - padTop - padBottom;
  const lastT = state.lastSample?.t ?? Date.now();
  const startT = lastT - windowS * 1000;
  const xFor = (t: number) => padLeft + clamp((t - startT) / (windowS * 1000), 0, 1) * plotW;
  const yForNorm = (norm: number) => padTop + (1 - clamp(norm, 0, 1)) * plotH;
  return (
    <div className="energyWindow">
      <div className="tempLegend">
        {series.map((item) => (
          <span key={item.key}>
            <i style={{ background: item.color }} />
            {item.label} {item.latest != null ? `${item.latest.toFixed(item.digits)} ${item.unit}` : "--"}
          </span>
        ))}
      </div>
      <svg className="energyChart vitalsChart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Speed, power and torque over the selected time window">
        <rect x={padLeft} y={padTop} width={plotW} height={plotH} rx="6" />
        <line x1={padLeft} x2={width - padRight} y1={height - padBottom} y2={height - padBottom} />
        <line x1={padLeft} x2={padLeft} y1={padTop} y2={height - padBottom} />
        <text x={padLeft} y={height - 7}>-{windowS}s</text>
        <text x={width - padRight - 28} y={height - 7}>now</text>
        {series.flatMap((item) => {
          const span = Math.max(1e-6, item.max - item.min);
          return item.segments.map((segment, index) => {
            const points = segment.map((point) => `${xFor(point.t).toFixed(1)},${yForNorm((point.value - item.min) / span).toFixed(1)}`).join(" ");
            return points ? <polyline key={`${item.key}-${index}`} points={points} style={{ stroke: item.color }} /> : null;
          });
        })}
      </svg>
    </div>
  );
}

function estimateP30bSoc(cellVoltage: number, cutoffCellV = DEFAULT_SOE_CUTOFF_CELL_V) {
  // Approximate Molicel P30B low-current discharge curve from the provided 23 C
  // capacity chart, expressed as usable energy remaining versus min-cell voltage.
  const baseCurve = [
    [2.8, 0],
    [2.95, 5],
    [3.10, 10],
    [3.25, 18],
    [3.40, 27],
    [3.55, 38],
    [3.68, 50],
    [3.80, 62],
    [3.92, 74],
    [4.02, 84],
    [4.10, 93],
    [4.2, 100],
  ] as const;
  const cutoff = clamp(cutoffCellV, 2, 4.15);
  const curve = [
    [cutoff, 0],
    ...baseCurve.filter(([voltage]) => voltage > cutoff),
  ] as Array<[number, number]>;
  if (cellVoltage <= curve[0][0]) return 0;
  for (let index = 1; index < curve.length; index += 1) {
    const [v1, soc1] = curve[index];
    const [v0, soc0] = curve[index - 1];
    if (cellVoltage <= v1) {
      const ratio = (cellVoltage - v0) / (v1 - v0);
      return clamp(soc0 + ratio * (soc1 - soc0), 0, 100);
    }
  }
  return 100;
}

function energyTrace(samples: LiveSample[], windowS: number) {
  const lastT = samples.at(-1)?.t;
  if (!lastT) return [];
  const startT = lastT - windowS * 1000;
  const windowSamples = samples.filter((sample) => sample.t >= startT);
  if (!windowSamples.length) return [];
  // Use VCU deltas (0x1C9) the same way the session accumulator does — no
  // client-side power×dt integration on the chart either.
  let energyOutWh = 0;
  let energyInWh = 0;
  const points = [{ t: windowSamples[0].t, energyWh: 0, energyOutWh: 0, energyInWh: 0 }];
  for (let index = 1; index < windowSamples.length; index += 1) {
    const previous = windowSamples[index - 1];
    const sample = windowSamples[index];
    const { outDeltaWh, inDeltaWh } = vcuEnergyDeltaWh(previous, sample);
    energyOutWh += outDeltaWh;
    energyInWh += inDeltaWh;
    points.push({ t: sample.t, energyWh: energyOutWh - energyInWh, energyOutWh, energyInWh });
  }
  return points;
}

function clamp(value: number, min: number, max: number) {
  return Math.max(min, Math.min(max, value));
}

function topLiveValues(values: Record<string, number>) {
  const priority = [
    "controls_motor_speed",
    "motor_speed",
    "dynamics_gps_speed",
    "gps_speed",
    "wheel_speed",
    "flw_speed",
    "frw_speed",
    "blw_speed",
    "brw_speed",
    "dynamics_inverter_rpm",
    "bus_voltage",
    "pack_dc_bus_v",
    "dc_bus_v",
    "pack_dc_bus_current",
    "dc_bus_current",
    "pack_hv_pack_v",
    "hv_pack_v",
    "pack_hv_c",
    "hv_c",
    "pack_hv_soc",
    "hv_soc",
    "power_kw",
    "controls_torque_feedback",
    "controls_torque_request",
    "dynamics_inverter_torque",
    "thermal_motor_temp",
    "thermal_inverter_temp",
  ];
  const entries = Object.entries(values).filter(([, value]) => Number.isFinite(value));
  const rank = new Map(priority.map((key, index) => [key, index]));
  return entries
    .sort(([a], [b]) => (rank.get(a) ?? 1000) - (rank.get(b) ?? 1000) || a.localeCompare(b))
    .slice(0, 12);
}

function rawAppsLiveRows(rawApps: ReturnType<typeof rawAppsTravelDetails>) {
  if (!rawApps) return [];
  const postDeadzone = applyAppsDeadzone(clamp(rawApps.average, 0, 1), 0.09, 0.88) * 100;
  return [
    ["raw_apps_avg_pct", rawApps.average * 100],
    ["post_deadzone_apps_pct", postDeadzone],
    ["raw_apps1_pct", rawApps.apps1Travel * 100],
    ["raw_apps2_pct", rawApps.apps2Travel * 100],
    ["raw_apps_diff_pct", (rawApps.apps1Travel - rawApps.apps2Travel) * 100],
  ] as [string, number][];
}

function labelFromKey(key: string) {
  return key.replace(/_/g, " ").replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function formatRawLiveValue(key: string, value: number) {
  const lower = key.toLowerCase();
  if (lower.includes("pct") || lower.includes("travel")) return `${value.toFixed(1)}%`;
  if (lower.includes("rpm") || lower.includes("motor_speed")) return `${Math.round(value).toLocaleString()} rpm`;
  if (lower.includes("speed")) return `${value.toFixed(2)} m/s`;
  if (lower.includes("voltage") || lower.endsWith("_v")) return `${value.toFixed(1)} V`;
  if (lower.includes("current") || lower.endsWith("_c")) return `${value.toFixed(1)} A`;
  if (lower.includes("power")) return `${value.toFixed(2)} kW`;
  if (lower.includes("temp")) return `${value.toFixed(1)} C`;
  if (lower.includes("torque")) return `${value.toFixed(1)} Nm`;
  return Math.abs(value) >= 100 ? Math.round(value).toLocaleString() : value.toFixed(3);
}

function formatValue(value: number) {
  if (Math.abs(value) >= 100) return Math.round(value).toString();
  return value.toFixed(1);
}

// ── First-load guided tour ────────────────────────────────────────────────────
// Lightweight, dependency-free onboarding overlay. Each step can focus a tab so
// users see the area being described. Fully skippable; dismissal is persisted by
// the caller (TOUR_STORAGE_KEY) so it only auto-opens once.
type TourStep = {
  tab?: AppTab;
  icon: ReactNode;
  title: string;
  body: ReactNode;
};

const TOUR_STEPS: TourStep[] = [
  {
    icon: <Radio size={20} />,
    title: "Welcome to Trackside Live",
    body: (
      <>
        This is the live telemetry dashboard for the car. It streams straight from
        the Kafka pipeline — gauges, strip charts, a GPS track map, and energy
        strategy all update in real time. This quick tour shows the main areas.
        You can skip it any time and reopen it from the <strong>?</strong> button.
      </>
    ),
  },
  {
    tab: "live",
    icon: <Gauge size={20} />,
    title: "Live Viewer",
    body: (
      <>
        The live tab <strong>auto-connects</strong> on load — no need to press
        Start. The hero shows lap time, best lap, energy used and regen. Gauges,
        the position map and pack/temperature panels fill in as data arrives. Use
        <strong> Reset</strong> in Live Setup to start a fresh session.
      </>
    ),
  },
  {
    tab: "live",
    icon: <Disc3 size={20} />,
    title: "Record a MoTeC file",
    body: (
      <>
        Hit <strong>Record</strong> to tag a start, then <strong>Stop &amp; Save</strong>{" "}
        to download a MoTeC <code>.ld</code>/<code>.ldx</code> of exactly that
        window — it works without track gates. The timer on the button shows the
        recording length. (<strong>Auto MoTeC on lap</strong> instead exports each
        completed lap automatically.)
      </>
    ),
  },
  {
    tab: "track-builder",
    icon: <MapPinned size={20} />,
    title: "Track Builder",
    body: (
      <>
        Draw the start/finish line and split gates on the map here. Once a track
        has a start/finish gate, the live viewer detects laps automatically and
        breaks them into sectors (best sectors highlight purple).
      </>
    ),
  },
  {
    tab: "dash",
    icon: <Gauge size={20} />,
    title: "Dash comms",
    body: (
      <>
        The <strong>Dash</strong> tab links to the on-car driver display: send a
        lap, broadcast the power budget, and push the start/finish line so the car
        counts laps itself. That&apos;s the tour — have fun!
      </>
    ),
  },
];

function TracksideTour({
  onNavigate,
  onClose,
  onFinish,
}: {
  onNavigate: (tab: AppTab) => void;
  onClose: () => void;
  onFinish: () => void;
}) {
  const [step, setStep] = useState(0);
  const current = TOUR_STEPS[step];
  const isFirst = step === 0;
  const isLast = step === TOUR_STEPS.length - 1;

  // Focus the tab this step describes so the user sees it behind the overlay.
  useEffect(() => {
    if (current.tab) onNavigate(current.tab);
  }, [step, current.tab, onNavigate]);

  const goNext = () => (isLast ? onFinish() : setStep((s) => s + 1));
  const goBack = () => setStep((s) => Math.max(0, s - 1));

  return (
    <div className="tourOverlay" role="dialog" aria-modal="true" aria-label="Guided tour">
      <div className="tourCard">
        <div className="tourHeader">
          <span className="tourIcon">{current.icon}</span>
          <h2>{current.title}</h2>
          <button className="tourClose" onClick={onClose} aria-label="Skip tour" title="Skip">
            <X size={18} />
          </button>
        </div>
        <p className="tourBody">{current.body}</p>
        <div className="tourDots" aria-hidden="true">
          {TOUR_STEPS.map((_, i) => (
            <span key={i} className={i === step ? "tourDot tourDotActive" : "tourDot"} />
          ))}
        </div>
        <div className="tourFooter">
          <button className="tool tourSkip" onClick={onClose}>
            Skip tour
          </button>
          <div className="tourNav">
            <button className="tool" onClick={goBack} disabled={isFirst}>
              <ChevronLeft size={15} /> Back
            </button>
            <button className="primary" onClick={goNext}>
              {isLast ? (
                <>
                  <GraduationCap size={15} /> Done
                </>
              ) : (
                <>
                  Next <ChevronRight size={15} />
                </>
              )}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

export default App;
