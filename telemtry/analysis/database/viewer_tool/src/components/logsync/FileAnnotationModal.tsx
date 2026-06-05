'use client';

// Trackside annotation modal for a single log file.
//
// Annotations are keyed by the file's loggerd name (orion_<startMs>.csv), not
// by job — the same file can belong to several jobs, and what an engineer
// records ("which run was this") belongs to the file. The modal is built for
// speed at the trackside: one-click flags up top, then notes/tags, then the
// structured identifiers. Saving PUTs the whole form to the worker.

import { useEffect, useRef, useState } from 'react';
import { toast } from 'react-toastify';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { cn } from '@/lib/utils';

export type AnnotationData = {
  name: string;
  notes: string;
  tags: string[];
  driver: string;
  track: string;
  session: string;
  weather: string;
  tires: string;
  setup: string;
  starred: boolean;
  quality: string; // '' | 'good' | 'bad' | 'corrupt'
  updated_ms: number;
};

export const emptyAnnotation = (name: string): AnnotationData => ({
  name, notes: '', tags: [],
  driver: '', track: '', session: '',
  weather: '', tires: '', setup: '',
  starred: false, quality: '', updated_ms: 0,
});

// True if the annotation carries any trackside signal — drives the inline
// badges/star in the file list.
export const hasAnnotation = (a?: AnnotationData | null): boolean =>
  !!a && (
    a.starred || !!a.quality || a.tags.length > 0 ||
    !!(a.notes || a.driver || a.track || a.session || a.weather || a.tires || a.setup)
  );

const QUALITY_OPTIONS: { value: string; label: string; cls: string }[] = [
  { value: 'good', label: 'Good', cls: 'bg-emerald-500/20 text-emerald-300 border-emerald-500/40' },
  { value: 'bad', label: 'Bad', cls: 'bg-amber-500/20 text-amber-300 border-amber-500/40' },
  { value: 'corrupt', label: 'Corrupt', cls: 'bg-red-500/20 text-red-300 border-red-500/40' },
];

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <label className="flex flex-col gap-1 text-sm">
      <span className="text-muted-foreground text-xs uppercase tracking-wide">{label}</span>
      {children}
    </label>
  );
}

export function FileAnnotationModal({
  fileName, subtitle, initial, onClose, onSaved,
}: {
  fileName: string;
  subtitle?: string;          // e.g. the file's local time range / size
  initial: AnnotationData;
  onClose: () => void;
  onSaved: (a: AnnotationData) => void;
}) {
  const [a, setA] = useState<AnnotationData>(initial);
  const [tagDraft, setTagDraft] = useState('');
  const [saving, setSaving] = useState(false);
  const firstRef = useRef<HTMLTextAreaElement | null>(null);

  const set = <K extends keyof AnnotationData>(k: K, v: AnnotationData[K]) =>
    setA((prev) => ({ ...prev, [k]: v }));

  // Esc to close; focus notes on open.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
    window.addEventListener('keydown', onKey);
    firstRef.current?.focus();
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose]);

  const commitTag = () => {
    // Split on whitespace/commas so a pasted "brake, endurance baseline" adds
    // three tags at once.
    const toAdd = tagDraft.split(/[\s,]+/).map((t) => t.trim())
      .filter((t) => t && !a.tags.includes(t));
    if (toAdd.length) set('tags', [...a.tags, ...toAdd]);
    setTagDraft('');
  };
  const removeTag = (t: string) => set('tags', a.tags.filter((x) => x !== t));

  const save = async () => {
    setSaving(true);
    try {
      const res = await fetch(`/api/logsync/annotations/${encodeURIComponent(fileName)}`, {
        method: 'PUT',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({
          notes: a.notes, tags: a.tags,
          driver: a.driver, track: a.track, session: a.session,
          weather: a.weather, tires: a.tires, setup: a.setup,
          starred: a.starred, quality: a.quality,
        }),
      });
      if (!res.ok) throw new Error((await res.json().catch(() => ({}))).detail ?? res.statusText);
      const saved: AnnotationData = await res.json();
      onSaved(saved);
      toast.success('Annotation saved');
      onClose();
    } catch (e) {
      toast.error(`Save failed: ${String(e)}`);
    } finally {
      setSaving(false);
    }
  };

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4"
      onMouseDown={onClose}
    >
      <div
        className="w-full max-w-2xl max-h-[90vh] overflow-y-auto rounded-xl border bg-card shadow-2xl"
        onMouseDown={(e) => e.stopPropagation()}
      >
        {/* header */}
        <div className="flex items-start justify-between gap-3 border-b p-5">
          <div className="min-w-0">
            <p className="font-mono text-sm truncate">{fileName}</p>
            {subtitle && <p className="text-xs text-muted-foreground mt-1">{subtitle}</p>}
          </div>
          <button
            onClick={() => set('starred', !a.starred)}
            title={a.starred ? 'Unstar' : 'Star this file'}
            className={cn(
              'shrink-0 rounded-md border px-2.5 py-1.5 text-lg leading-none transition-colors',
              a.starred
                ? 'bg-yellow-500/20 text-yellow-300 border-yellow-500/40'
                : 'text-muted-foreground hover:text-foreground',
            )}
          >
            {a.starred ? '★' : '☆'}
          </button>
        </div>

        <div className="flex flex-col gap-5 p-5">
          {/* quick triage: data quality */}
          <Field label="Data quality">
            <div className="flex flex-wrap gap-2">
              {QUALITY_OPTIONS.map((q) => {
                const on = a.quality === q.value;
                return (
                  <button
                    key={q.value}
                    onClick={() => set('quality', on ? '' : q.value)}
                    className={cn(
                      'rounded-full border px-3 py-1 text-xs transition-colors',
                      on ? q.cls : 'text-muted-foreground border-border hover:text-foreground',
                    )}
                  >
                    {q.label}
                  </button>
                );
              })}
            </div>
          </Field>

          {/* notes */}
          <Field label="Notes">
            <textarea
              ref={firstRef}
              value={a.notes}
              onChange={(e) => set('notes', e.target.value)}
              rows={3}
              placeholder="What happened on this run? Issues, observations, things to look at…"
              className="rounded-md border bg-transparent px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-ring resize-y"
            />
          </Field>

          {/* tags */}
          <Field label="Tags">
            <div className="flex flex-wrap items-center gap-2 rounded-md border px-2 py-2">
              {a.tags.map((t) => (
                <span key={t} className="flex items-center gap-1 rounded-full bg-primary/15 text-primary border border-primary/30 px-2 py-0.5 text-xs">
                  {t}
                  <button onClick={() => removeTag(t)} className="hover:text-foreground" title="Remove">×</button>
                </span>
              ))}
              <input
                value={tagDraft}
                onChange={(e) => setTagDraft(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter' || e.key === ',') { e.preventDefault(); commitTag(); }
                  else if (e.key === 'Backspace' && !tagDraft && a.tags.length) removeTag(a.tags[a.tags.length - 1]);
                }}
                onBlur={commitTag}
                placeholder={a.tags.length ? 'Add tag…' : 'e.g. endurance, brake-test, baseline'}
                className="flex-1 min-w-[8rem] bg-transparent text-sm outline-none"
              />
            </div>
          </Field>

          {/* run context */}
          <div>
            <p className="text-xs uppercase tracking-wide text-muted-foreground mb-2">Run context</p>
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
              <Field label="Driver"><Input value={a.driver} onChange={(e) => set('driver', e.target.value)} placeholder="Name" /></Field>
              <Field label="Track"><Input value={a.track} onChange={(e) => set('track', e.target.value)} placeholder="Track / venue" /></Field>
              <Field label="Session / run"><Input value={a.session} onChange={(e) => set('session', e.target.value)} placeholder="e.g. AM endurance, run 3" /></Field>
            </div>
          </div>

          {/* conditions */}
          <div>
            <p className="text-xs uppercase tracking-wide text-muted-foreground mb-2">Conditions</p>
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
              <Field label="Weather"><Input value={a.weather} onChange={(e) => set('weather', e.target.value)} placeholder="Dry, 28°C…" /></Field>
              <Field label="Tires"><Input value={a.tires} onChange={(e) => set('tires', e.target.value)} placeholder="Compound / set" /></Field>
              <Field label="Setup"><Input value={a.setup} onChange={(e) => set('setup', e.target.value)} placeholder="Setup change" /></Field>
            </div>
          </div>
        </div>

        {/* footer */}
        <div className="flex items-center justify-between gap-3 border-t p-5">
          <span className="text-xs text-muted-foreground">
            {a.updated_ms ? `Last saved ${new Date(a.updated_ms).toLocaleString()}` : 'Not yet saved'}
          </span>
          <div className="flex items-center gap-2">
            <Button variant="outline" onClick={onClose} disabled={saving}>Cancel</Button>
            <Button onClick={save} disabled={saving}>{saving ? 'Saving…' : 'Save'}</Button>
          </div>
        </div>
      </div>
    </div>
  );
}
