'use client';

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { toast } from 'react-toastify';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { cn } from '@/lib/utils';
import {
  FileAnnotationModal,
  type AnnotationData,
  emptyAnnotation,
  hasAnnotation,
} from '@/components/logsync/FileAnnotationModal';
import { FilePreviewModal } from '@/components/logsync/FilePreviewModal';
import { fetchSessionsFromServer, parseLogStartMs, type TracksideSessionInfo } from '@/lib/trackside/sessionRegistry';

// ---- types mirrored from the worker's Job.to_dict() ----
type FileProgress = { name: string; size: number; transferred: number; done: boolean; start_ms?: number; end_ms?: number };
type Motion = { moving: boolean; speed_mps: number | null; sample_age_ms: number | null; recent: boolean; error?: string | null };
type Job = {
  id: string;
  from_ms: number;
  to_ms: number;
  bwlimit_kbps: number;
  state: string;
  created_ms: number;
  updated_ms: number;
  files: FileProgress[];
  total_bytes: number;
  transferred_bytes: number;
  rate_bps: number;
  attempts: number;
  error: string | null;
  last_motion: Motion | null;
  percent: number;
  file_count: number;
  files_done: number;
};

// ---- formatting helpers ----
const fmtBytes = (b: number) => {
  if (!b) return '0 B';
  const u = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(b) / Math.log(1024));
  return `${(b / 1024 ** i).toFixed(i ? 1 : 0)} ${u[i]}`;
};
const fmtRate = (bps: number) => (bps > 0 ? `${fmtBytes(bps)}/s` : '—');
const fmtTime = (ms: number) => new Date(ms).toLocaleString();
// Session start parsed from the loggerd filename (orion_<startMs>.csv) as a
// fallback for jobs created before start_ms/end_ms were stored.
const startMsFromName = (name: string): number => {
  const m = name.match(/_(\d{10,13})\./);   // a millisecond epoch (through ~year 2286)
  const ms = m ? Number(m[1]) : NaN;
  return Number.isFinite(ms) ? ms : 0;
};
// "start → end" in the viewer's local timezone. End (file mtime) shows as time
// only when it's the same day as start; omitted entirely if unknown (0).
const fileTimeRange = (f: FileProgress): string => {
  const start = f.start_ms && f.start_ms > 0 ? f.start_ms : startMsFromName(f.name);
  if (!start) return '';
  const startStr = new Date(start).toLocaleString();
  const end = f.end_ms && f.end_ms > start ? f.end_ms : 0;
  if (!end) return startStr;
  const sameDay = new Date(start).toDateString() === new Date(end).toDateString();
  return `${startStr} → ${sameDay ? new Date(end).toLocaleTimeString() : new Date(end).toLocaleString()}`;
};
const toLocalInput = (d: Date) => {
  const p = (n: number) => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}T${p(d.getHours())}:${p(d.getMinutes())}`;
};

const STATE_STYLE: Record<string, string> = {
  queued: 'bg-blue-500/15 text-blue-300 border-blue-500/30',
  running: 'bg-green-500/15 text-green-300 border-green-500/30',
  paused_motion: 'bg-amber-500/15 text-amber-300 border-amber-500/30',
  paused: 'bg-zinc-500/15 text-zinc-300 border-zinc-500/30',
  completed: 'bg-emerald-500/15 text-emerald-300 border-emerald-500/30',
  failed: 'bg-red-500/15 text-red-300 border-red-500/30',
  canceled: 'bg-zinc-600/15 text-zinc-400 border-zinc-600/30',
};
const STATE_LABEL: Record<string, string> = {
  queued: 'Queued',
  running: 'Transferring',
  paused_motion: 'Paused — car moving',
  paused: 'Paused',
  completed: 'Completed',
  failed: 'Failed',
  canceled: 'Canceled',
};

async function post(path: string) {
  const res = await fetch(`/api/logsync/${path}`, { method: 'POST' });
  if (!res.ok) throw new Error((await res.json().catch(() => ({}))).detail ?? res.statusText);
  return res.json();
}

export default function LogSyncPage() {
  const now = useMemo(() => new Date(), []);
  const [from, setFrom] = useState(() => toLocalInput(new Date(Date.now() - 60 * 60 * 1000)));
  const [to, setTo] = useState(() => toLocalInput(now));
  const [bwlimit, setBwlimit] = useState('');
  const [preview, setPreview] = useState<{ count: number; total_bytes: number } | null>(null);
  const [busy, setBusy] = useState(false);
  const [jobs, setJobs] = useState<Record<string, Job>>({});
  const [motion, setMotion] = useState<Motion | null>(null);
  const [expanded, setExpanded] = useState<Set<string>>(new Set());
  // Per-file trackside annotations, keyed by loggerd filename (shared across jobs).
  const [annotations, setAnnotations] = useState<Record<string, AnnotationData>>({});
  const [modalFile, setModalFile] = useState<{ name: string; subtitle: string } | null>(null);
  const [previewFile, setPreviewFile] = useState<{ jobId: string; name: string; subtitle: string } | null>(null);
  // 'jobs' = per-job cards; 'files' = flat deduped list of every file.
  const [view, setView] = useState<'jobs' | 'files'>('jobs');
  // File search / filtering across all jobs.
  const [query, setQuery] = useState('');
  const [starredOnly, setStarredOnly] = useState(false);
  const [qualityFilter, setQualityFilter] = useState<Set<string>>(new Set());
  const [tagFilter, setTagFilter] = useState<Set<string>>(new Set());
  // Logged trackside sessions, for "query by the sessions we logged".
  const [sessions, setSessions] = useState<TracksideSessionInfo[]>([]);
  const [sessionFilter, setSessionFilter] = useState<string>(''); // session id, '' = all
  const esRef = useRef<EventSource | null>(null);

  const fromMs = () => new Date(from).getTime();
  const toMs = () => new Date(to).getTime();

  // Live job progress via SSE.
  useEffect(() => {
    const es = new EventSource('/api/logsync/events');
    esRef.current = es;
    es.addEventListener('snapshot', (e) => {
      const arr: Job[] = JSON.parse((e as MessageEvent).data);
      setJobs(Object.fromEntries(arr.map((j) => [j.id, j])));
    });
    es.onmessage = (e) => {
      const job: Job = JSON.parse(e.data);
      setJobs((prev) => ({ ...prev, [job.id]: job }));
    };
    es.onerror = () => {/* browser auto-reconnects */};
    return () => es.close();
  }, []);

  // Lightweight global motion chip.
  useEffect(() => {
    let alive = true;
    const tick = async () => {
      try {
        const r = await fetch('/api/logsync/motion');
        if (alive && r.ok) setMotion(await r.json());
      } catch {/* ignore */}
    };
    tick();
    const id = setInterval(tick, 5000);
    return () => { alive = false; clearInterval(id); };
  }, []);

  // Load all file annotations once; merged into the file rows by filename.
  useEffect(() => {
    let alive = true;
    (async () => {
      try {
        const r = await fetch('/api/logsync/annotations');
        if (alive && r.ok) setAnnotations(await r.json());
      } catch {/* annotations are non-critical; ignore */}
    })();
    return () => { alive = false; };
  }, []);

  // Load the logged trackside sessions for the "query by session" filter.
  useEffect(() => {
    let alive = true;
    void fetchSessionsFromServer().then((s) => { if (alive) setSessions(s); }).catch(() => {});
    return () => { alive = false; };
  }, []);

  const doPreview = async () => {
    const f = fromMs(), t = toMs();
    if (Number.isNaN(f) || Number.isNaN(t)) { toast.error('Invalid start or end time'); return; }
    setBusy(true);
    try {
      const r = await fetch(`/api/logsync/logs?from_ms=${f}&to_ms=${t}`);
      if (!r.ok) throw new Error((await r.json().catch(() => ({}))).detail ?? r.statusText);
      const data = await r.json();
      setPreview({ count: data.count, total_bytes: data.total_bytes });
    } catch (e) {
      toast.error(`Preview failed: ${String(e)}`);
      setPreview(null);
    } finally {
      setBusy(false);
    }
  };

  const startJob = async () => {
    const f = fromMs(), t = toMs();
    if (Number.isNaN(f) || Number.isNaN(t)) { toast.error('Invalid start or end time'); return; }
    if (t <= f) { toast.error('End time must be after start time'); return; }
    setBusy(true);
    try {
      const body: Record<string, number> = { from_ms: f, to_ms: t };
      if (bwlimit.trim()) body.bwlimit_kbps = Number(bwlimit);
      const res = await fetch('/api/logsync/jobs', {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify(body),
      });
      if (!res.ok) throw new Error((await res.json().catch(() => ({}))).detail ?? res.statusText);
      const job: Job = await res.json();
      setJobs((prev) => ({ ...prev, [job.id]: job }));
      toast.success(`Started sync of ${job.file_count} file(s), ${fmtBytes(job.total_bytes)}`);
    } catch (e) {
      toast.error(`Could not start: ${String(e)}`);
    } finally {
      setBusy(false);
    }
  };

  const control = async (id: string, action: 'pause' | 'resume' | 'cancel') => {
    try {
      await post(`jobs/${id}/${action}`);
    } catch (e) {
      toast.error(`${action} failed: ${String(e)}`);
    }
  };

  const toggleExpand = (id: string) =>
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id); else next.add(id);
      return next;
    });

  const fileSubtitle = (f: FileProgress) => [fileTimeRange(f), fmtBytes(f.size)].filter(Boolean).join(' · ');
  const openFile = (f: FileProgress) => setModalFile({ name: f.name, subtitle: fileSubtitle(f) });
  const openPreview = (jobId: string, f: FileProgress) =>
    setPreviewFile({ jobId, name: f.name, subtitle: fileSubtitle(f) });

  // Every tag in use, for the filter chips.
  const allTags = useMemo(() => {
    const s = new Set<string>();
    Object.values(annotations).forEach((a) => a.tags.forEach((t) => s.add(t)));
    return [...s].sort();
  }, [annotations]);

  const filterActive = query.trim() !== '' || starredOnly || qualityFilter.size > 0 || tagFilter.size > 0 || sessionFilter !== '';

  // Does a file pass the active filters? Matches filename + all annotation text,
  // and (when a logged session is picked) the file's loggerd timestamp falling
  // inside that session's start–end window.
  const matchFile = useCallback((name: string): boolean => {
    const a = annotations[name];
    if (starredOnly && !a?.starred) return false;
    if (qualityFilter.size > 0 && !(a && qualityFilter.has(a.quality))) return false;
    if (tagFilter.size > 0 && !(a && [...tagFilter].some((t) => a.tags.includes(t)))) return false;
    if (sessionFilter) {
      const s = sessions.find((x) => x.id === sessionFilter);
      const ms = parseLogStartMs(name);
      if (!s || ms == null) return false;
      const end = s.endedAt ?? s.startedAt + 6 * 60 * 60 * 1000; // open session: cap window at 6h
      if (ms < s.startedAt || ms > end) return false;
    }
    const q = query.trim().toLowerCase();
    if (q) {
      const hay = [name, a?.notes, a?.driver, a?.track, a?.session, a?.weather, a?.tires, a?.setup, ...(a?.tags ?? [])]
        .filter(Boolean).join(' ').toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  }, [annotations, query, starredOnly, qualityFilter, tagFilter, sessionFilter, sessions]);

  const toggleSetItem = (set: Set<string>, setter: (s: Set<string>) => void, v: string) => {
    const next = new Set(set);
    next.has(v) ? next.delete(v) : next.add(v);
    setter(next);
  };
  const clearFilters = () => { setQuery(''); setStarredOnly(false); setQualityFilter(new Set()); setTagFilter(new Set()); setSessionFilter(''); };

  const jobList = Object.values(jobs).sort((a, b) => b.created_ms - a.created_ms);
  const matchCount = useMemo(
    () => (filterActive ? jobList.reduce((n, j) => n + j.files.filter((f) => matchFile(f.name)).length, 0) : 0),
    [filterActive, jobList, matchFile],
  );

  // Every distinct file across all jobs (the shared store means one file can
  // appear in several jobs). Deduped by name, preferring a job that has the
  // file completed so preview/download works, and sorted newest-session-first.
  const allFiles = useMemo(() => {
    const m = new Map<string, { f: FileProgress; jobId: string }>();
    for (const job of jobList) {
      for (const f of job.files) {
        const seen = m.get(f.name);
        if (!seen || (!seen.f.done && f.done)) m.set(f.name, { f, jobId: job.id });
      }
    }
    const startMs = (f: FileProgress) => (f.start_ms && f.start_ms > 0 ? f.start_ms : startMsFromName(f.name));
    return [...m.values()].sort((a, b) => startMs(b.f) - startMs(a.f));
  }, [jobList]);
  const visibleAllFiles = filterActive ? allFiles.filter((x) => matchFile(x.f.name)) : allFiles;

  return (
    <div className="min-h-screen pt-20 px-6 max-w-5xl mx-auto">
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-3xl font-bold">Log Sync</h1>
        <MotionChip motion={motion} />
      </div>
      <p className="text-sm text-muted-foreground mb-6">
        Pull <code>loggerd</code> CSVs from the BEVO Pi for a time range. Transfers pause
        automatically while the car is moving and resume when it stops.
      </p>

      {/* New job form */}
      <div className="rounded-lg border p-5 mb-8 bg-card/30">
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <label className="flex flex-col gap-1 text-sm">
            <span className="text-muted-foreground">From</span>
            <Input type="datetime-local" value={from} onChange={(e) => { setFrom(e.target.value); setPreview(null); }} />
          </label>
          <label className="flex flex-col gap-1 text-sm">
            <span className="text-muted-foreground">To</span>
            <Input type="datetime-local" value={to} onChange={(e) => { setTo(e.target.value); setPreview(null); }} />
          </label>
          <label className="flex flex-col gap-1 text-sm">
            <span className="text-muted-foreground">Bandwidth cap (KB/s, optional)</span>
            <Input type="number" min={0} placeholder="unlimited" value={bwlimit} onChange={(e) => setBwlimit(e.target.value)} />
          </label>
        </div>
        <div className="flex items-center gap-3 mt-4">
          {(() => {
            const rangeValid = !!from && !!to && new Date(from).getTime() < new Date(to).getTime();
            const reason = busy ? "Working…" : !from || !to ? "Set a From and To time" : !rangeValid ? "“To” must be after “From”" : undefined;
            return (
              <>
                <Button variant="outline" onClick={doPreview} disabled={busy || !rangeValid} title={reason}>Preview</Button>
                <Button onClick={startJob} disabled={busy || !rangeValid} title={reason}>Start sync</Button>
                {reason && !busy ? <span className="text-sm text-muted-foreground">{reason}</span> : null}
              </>
            );
          })()}
          {preview && (
            <span className="text-sm text-muted-foreground">
              {preview.count === 0
                ? 'No files overlap this range'
                : `${preview.count} file(s) · ${fmtBytes(preview.total_bytes)}`}
            </span>
          )}
        </div>
      </div>

      {/* Jobs / files */}
      <div className="flex items-center justify-between mb-3 gap-3 flex-wrap">
        <div className="flex items-center gap-3">
          <h2 className="text-xl font-semibold">{view === 'jobs' ? 'Jobs' : 'All files'}</h2>
          {jobList.length > 0 && (
            <div className="flex rounded-md border overflow-hidden text-xs">
              {(['jobs', 'files'] as const).map((v) => (
                <button
                  key={v}
                  onClick={() => setView(v)}
                  className={cn('px-3 py-1 transition-colors', view === v ? 'bg-primary/20 text-primary' : 'text-muted-foreground hover:text-foreground')}
                >
                  {v === 'jobs' ? 'By job' : 'All files'}
                </button>
              ))}
            </div>
          )}
        </div>
        {filterActive && (
          <span className="text-xs text-muted-foreground">
            {view === 'files' ? visibleAllFiles.length : matchCount} file{(view === 'files' ? visibleAllFiles.length : matchCount) === 1 ? '' : 's'} match
          </span>
        )}
      </div>

      {/* File search / filters */}
      {jobList.length > 0 && (
        <div className="rounded-lg border p-3 mb-4 bg-card/30 flex flex-col gap-3">
          <div className="flex items-center gap-2">
            <Input
              placeholder="Search files — name, notes, tags, driver, track…"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
            />
            {filterActive && (
              <Button variant="ghost" size="sm" onClick={clearFilters}>Clear</Button>
            )}
          </div>
          <div className="flex items-center gap-2 flex-wrap">
            {sessions.length > 0 && (
              <select
                className="rounded-md border bg-background px-2 py-1 text-sm"
                value={sessionFilter}
                onChange={(e) => setSessionFilter(e.target.value)}
                title="Show only files logged during a trackside session"
              >
                <option value="">All sessions</option>
                {sessions.slice().sort((a, b) => b.startedAt - a.startedAt).map((s) => (
                  <option key={s.id} value={s.id}>
                    {s.name} · {s.car} · {new Date(s.startedAt).toLocaleString([], { month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit' })}
                  </option>
                ))}
              </select>
            )}
            {sessions.length > 0 && <span className="mx-1 h-4 w-px bg-border" />}
            <FilterChip on={starredOnly} onClick={() => setStarredOnly((v) => !v)}>★ Starred</FilterChip>
            {(['good', 'bad', 'corrupt'] as const).map((q) => (
              <FilterChip key={q} on={qualityFilter.has(q)} onClick={() => toggleSetItem(qualityFilter, setQualityFilter, q)}>
                {q[0].toUpperCase() + q.slice(1)}
              </FilterChip>
            ))}
            {allTags.length > 0 && <span className="mx-1 h-4 w-px bg-border" />}
            {allTags.map((t) => (
              <FilterChip key={t} on={tagFilter.has(t)} onClick={() => toggleSetItem(tagFilter, setTagFilter, t)}>#{t}</FilterChip>
            ))}
          </div>
        </div>
      )}

      {jobList.length === 0 && <p className="text-sm text-muted-foreground">No jobs yet.</p>}

      {view === 'jobs' ? (
        <>
          {filterActive && matchCount === 0 && (
            <p className="text-sm text-muted-foreground">No files match the current filters.</p>
          )}
          <div className="flex flex-col gap-3">
            {jobList.map((job) => (
              <JobCard
                key={job.id}
                job={job}
                expanded={expanded.has(job.id) || filterActive}
                onToggle={() => toggleExpand(job.id)}
                onControl={control}
                annotations={annotations}
                onOpenFile={openFile}
                onPreviewFile={openPreview}
                fileFilter={filterActive ? matchFile : null}
              />
            ))}
          </div>
        </>
      ) : (
        <div className="rounded-lg border bg-card/30 divide-y">
          {visibleAllFiles.length === 0 ? (
            <p className="text-sm text-muted-foreground p-4">
              {filterActive ? 'No files match the current filters.' : 'No files yet.'}
            </p>
          ) : (
            visibleAllFiles.map(({ f, jobId }) => (
              <div key={f.name} className="px-4 py-2">
                <FileRow jobId={jobId} f={f} ann={annotations[f.name]} onOpenFile={openFile} onPreviewFile={openPreview} />
              </div>
            ))
          )}
        </div>
      )}

      {modalFile && (
        <FileAnnotationModal
          fileName={modalFile.name}
          subtitle={modalFile.subtitle}
          initial={annotations[modalFile.name] ?? emptyAnnotation(modalFile.name)}
          onClose={() => setModalFile(null)}
          onSaved={(saved) =>
            setAnnotations((prev) => {
              // An emptied annotation comes back blank; drop it so badges clear.
              const next = { ...prev };
              if (hasAnnotation(saved)) next[saved.name] = saved;
              else delete next[saved.name];
              return next;
            })
          }
        />
      )}

      {previewFile && (
        <FilePreviewModal
          jobId={previewFile.jobId}
          fileName={previewFile.name}
          subtitle={previewFile.subtitle}
          onClose={() => setPreviewFile(null)}
        />
      )}
    </div>
  );
}

function FilterChip({ on, onClick, children }: { on: boolean; onClick: () => void; children: React.ReactNode }) {
  return (
    <button
      onClick={onClick}
      className={cn(
        'rounded-full border px-2.5 py-0.5 text-xs transition-colors',
        on ? 'bg-primary/20 text-primary border-primary/40' : 'text-muted-foreground border-border hover:text-foreground',
      )}
    >
      {children}
    </button>
  );
}

function MotionChip({ motion }: { motion: Motion | null }) {
  if (!motion) return null;
  const moving = motion.moving;
  const cls = moving
    ? 'bg-amber-500/15 text-amber-300 border-amber-500/30'
    : motion.recent
      ? 'bg-green-500/15 text-green-300 border-green-500/30'
      : 'bg-zinc-500/15 text-zinc-300 border-zinc-500/30';
  const label = moving
    ? `Car moving${motion.speed_mps != null ? ` (${motion.speed_mps.toFixed(1)} m/s)` : ''}`
    : motion.recent ? 'Car stationary' : 'No recent telemetry';
  return <span className={`text-xs px-3 py-1 rounded-full border ${cls}`}>{label}</span>;
}

function JobCard({
  job, expanded, onToggle, onControl, annotations, onOpenFile, onPreviewFile, fileFilter,
}: {
  job: Job;
  expanded: boolean;
  onToggle: () => void;
  onControl: (id: string, a: 'pause' | 'resume' | 'cancel') => void;
  annotations: Record<string, AnnotationData>;
  onOpenFile: (f: FileProgress) => void;
  onPreviewFile: (jobId: string, f: FileProgress) => void;
  fileFilter: ((name: string) => boolean) | null;
}) {
  const active = !['completed', 'failed', 'canceled'].includes(job.state);
  const canPause = ['running', 'queued', 'paused_motion'].includes(job.state);
  const canResume = ['paused'].includes(job.state);
  const hasDownloads = job.files_done > 0;
  const visibleFiles = fileFilter ? job.files.filter((f) => fileFilter(f.name)) : job.files;

  // When a filter is active, a job with no matching files drops out entirely.
  if (fileFilter && visibleFiles.length === 0) return null;

  return (
    <div className="rounded-lg border p-4 bg-card/30">
      <div className="flex items-center justify-between gap-3 flex-wrap">
        <div className="flex items-center gap-3">
          <span className={`text-xs px-2.5 py-1 rounded-full border ${STATE_STYLE[job.state] ?? ''}`}>
            {STATE_LABEL[job.state] ?? job.state}
          </span>
          <span className="text-sm text-muted-foreground">
            {fmtTime(job.from_ms)} → {fmtTime(job.to_ms)}
          </span>
        </div>
        <div className="flex items-center gap-2">
          {canPause && <Button size="sm" variant="outline" onClick={() => onControl(job.id, 'pause')}>Pause</Button>}
          {canResume && <Button size="sm" variant="outline" onClick={() => onControl(job.id, 'resume')}>Resume</Button>}
          {active && <Button size="sm" variant="destructive" onClick={() => onControl(job.id, 'cancel')}>Cancel</Button>}
          {hasDownloads && (
            <a href={`/api/logsync/jobs/${job.id}/archive`}>
              <Button size="sm">Download .zip</Button>
            </a>
          )}
        </div>
      </div>

      {/* progress bar */}
      <div className="mt-3">
        <div className="h-2 w-full rounded-full bg-muted overflow-hidden">
          <div
            className={`h-full ${job.state === 'failed' ? 'bg-red-500' : 'bg-primary'} transition-all`}
            style={{ width: `${job.percent}%` }}
          />
        </div>
        <div className="flex items-center justify-between mt-1 text-xs text-muted-foreground">
          <span>
            {job.percent.toFixed(1)}% · {fmtBytes(job.transferred_bytes)} / {fmtBytes(job.total_bytes)} ·{' '}
            {job.files_done}/{job.file_count} files
          </span>
          <span>
            {job.state === 'running' && `${fmtRate(job.rate_bps)} · `}
            <button className="underline hover:text-foreground" onClick={onToggle}>
              {expanded ? 'hide files' : 'files'}
            </button>
          </span>
        </div>
      </div>

      {job.state === 'paused_motion' && job.last_motion?.speed_mps != null && (
        <p className="text-xs text-amber-300/80 mt-2">
          Waiting for the car to stop — current speed {job.last_motion.speed_mps.toFixed(1)} m/s.
        </p>
      )}
      {job.error && <p className="text-xs text-red-400 mt-2">{job.error}</p>}

      {expanded && (
        <div className="mt-3 border-t pt-3 flex flex-col gap-1">
          {visibleFiles.map((f) => (
            <FileRow
              key={f.name}
              jobId={job.id}
              f={f}
              ann={annotations[f.name]}
              onOpenFile={onOpenFile}
              onPreviewFile={onPreviewFile}
            />
          ))}
        </div>
      )}
    </div>
  );
}

// One file row: clickable name (→ annotation modal) with inline badges, time
// range, size, and a preview/download action. Shared by the per-job file list
// and the flat "All files" view.
function FileRow({
  jobId, f, ann, onOpenFile, onPreviewFile,
}: {
  jobId: string;
  f: FileProgress;
  ann?: AnnotationData;
  onOpenFile: (f: FileProgress) => void;
  onPreviewFile: (jobId: string, f: FileProgress) => void;
}) {
  const annotated = hasAnnotation(ann);
  return (
    <div className="flex items-center justify-between text-xs gap-3">
      <button
        onClick={() => onOpenFile(f)}
        title="Add or edit trackside notes"
        className="group flex items-center gap-2 min-w-0 text-left"
      >
        {ann?.starred && <span className="text-yellow-400 shrink-0">★</span>}
        <span className="font-mono truncate group-hover:text-foreground group-hover:underline">{f.name}</span>
        {annotated
          ? <FileBadges ann={ann!} />
          : <span className="shrink-0 opacity-0 group-hover:opacity-100 transition-opacity text-muted-foreground">✎ annotate</span>}
      </button>
      <span className="flex items-center gap-3 shrink-0 text-muted-foreground">
        <span className="hidden sm:inline tabular-nums">{fileTimeRange(f)}</span>
        <span>{fmtBytes(f.size)}</span>
        {f.done
          ? <button className="underline hover:text-foreground" onClick={() => onPreviewFile(jobId, f)}>preview / download</button>
          : <span title="still transferring">…</span>}
      </span>
    </div>
  );
}

// Inline annotation summary shown next to a file name: quality dot, a couple of
// tags, and a notes indicator — enough to tell at a glance "what this file is".
const QUALITY_DOT: Record<string, string> = {
  good: 'bg-emerald-400', bad: 'bg-amber-400', corrupt: 'bg-red-400',
};
function FileBadges({ ann }: { ann: AnnotationData }) {
  return (
    <span className="flex items-center gap-1.5 shrink-0 min-w-0">
      {ann.quality && (
        <span title={`Quality: ${ann.quality}`} className={`h-1.5 w-1.5 rounded-full ${QUALITY_DOT[ann.quality] ?? 'bg-zinc-400'}`} />
      )}
      {ann.tags.slice(0, 2).map((t) => (
        <span key={t} className="rounded-full bg-primary/15 text-primary border border-primary/30 px-1.5 py-px max-w-[7rem] truncate">{t}</span>
      ))}
      {ann.tags.length > 2 && <span className="text-muted-foreground">+{ann.tags.length - 2}</span>}
      {ann.notes && <span title={ann.notes} className="text-muted-foreground">📝</span>}
    </span>
  );
}
