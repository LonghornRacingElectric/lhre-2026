'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import { toast } from 'react-toastify';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';

// ---- types mirrored from the worker's Job.to_dict() ----
type FileProgress = { name: string; size: number; transferred: number; done: boolean };
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

  const jobList = Object.values(jobs).sort((a, b) => b.created_ms - a.created_ms);

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
          <Button variant="outline" onClick={doPreview} disabled={busy}>Preview</Button>
          <Button onClick={startJob} disabled={busy}>Start sync</Button>
          {preview && (
            <span className="text-sm text-muted-foreground">
              {preview.count === 0
                ? 'No files overlap this range'
                : `${preview.count} file(s) · ${fmtBytes(preview.total_bytes)}`}
            </span>
          )}
        </div>
      </div>

      {/* Jobs */}
      <h2 className="text-xl font-semibold mb-3">Jobs</h2>
      {jobList.length === 0 && <p className="text-sm text-muted-foreground">No jobs yet.</p>}
      <div className="flex flex-col gap-3">
        {jobList.map((job) => (
          <JobCard
            key={job.id}
            job={job}
            expanded={expanded.has(job.id)}
            onToggle={() => toggleExpand(job.id)}
            onControl={control}
          />
        ))}
      </div>
    </div>
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
  job, expanded, onToggle, onControl,
}: {
  job: Job;
  expanded: boolean;
  onToggle: () => void;
  onControl: (id: string, a: 'pause' | 'resume' | 'cancel') => void;
}) {
  const active = !['completed', 'failed', 'canceled'].includes(job.state);
  const canPause = ['running', 'queued', 'paused_motion'].includes(job.state);
  const canResume = ['paused'].includes(job.state);
  const hasDownloads = job.files_done > 0;

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
          {job.files.map((f) => (
            <div key={f.name} className="flex items-center justify-between text-xs">
              <span className="font-mono truncate mr-3">{f.name}</span>
              <span className="flex items-center gap-3 shrink-0 text-muted-foreground">
                <span>{fmtBytes(f.transferred)} / {fmtBytes(f.size)}</span>
                {f.done ? (
                  <a className="underline hover:text-foreground"
                     href={`/api/logsync/jobs/${job.id}/files/${encodeURIComponent(f.name)}`}>
                    download
                  </a>
                ) : (
                  <span>{job.total_bytes ? '…' : ''}</span>
                )}
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
