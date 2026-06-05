'use client';

// Preview a log file's first rows and pick which columns to download.
//
// loggerd CSVs carry hundreds of columns (140 cell voltages, 92 cell temps,
// every dynamics/controls/thermal channel…). Rather than pull a multi-GB file
// to look at three signals, this modal fetches just the header + first rows,
// lets the engineer tick the columns they want (grouped by subsystem, with a
// sample value to identify each), and downloads a CSV projected to just those
// columns server-side.

import { useEffect, useMemo, useState } from 'react';
import { toast } from 'react-toastify';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { cn } from '@/lib/utils';

type Head = { columns: string[]; rows: string[][]; total_columns: number; returned_rows: number };

// Subsystem a column belongs to: the bit before the first '.' or '['. Scalars
// like `time` / `packet_id` (no separator) go under a "(top-level)" group.
const groupOf = (col: string): string => {
  const base = col.match(/^[^.[]+/)?.[0] ?? col;
  return /[.[]/.test(col) ? base : '(top-level)';
};

// Columns worth pre-selecting so a fresh preview is immediately downloadable.
const DEFAULT_COLS = ['time', 'packet_id'];

export function FilePreviewModal({
  jobId, fileName, subtitle, onClose,
}: {
  jobId: string;
  fileName: string;
  subtitle?: string;
  onClose: () => void;
}) {
  const [head, setHead] = useState<Head | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [selected, setSelected] = useState<Set<string>>(new Set());
  const [colSearch, setColSearch] = useState('');

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose]);

  useEffect(() => {
    let alive = true;
    (async () => {
      try {
        const r = await fetch(`/api/logsync/jobs/${jobId}/files/${encodeURIComponent(fileName)}/head?rows=10`);
        if (!r.ok) throw new Error((await r.json().catch(() => ({}))).detail ?? r.statusText);
        const h: Head = await r.json();
        if (!alive) return;
        setHead(h);
        setSelected(new Set(DEFAULT_COLS.filter((c) => h.columns.includes(c))));
      } catch (e) {
        if (alive) setError(String(e));
      }
    })();
    return () => { alive = false; };
  }, [jobId, fileName]);

  // First-row sample value per column, to help identify what each channel is.
  const sample = useMemo(() => {
    const m: Record<string, string> = {};
    if (head?.rows[0]) head.columns.forEach((c, i) => { m[c] = head.rows[0][i] ?? ''; });
    return m;
  }, [head]);

  // Columns matching the picker search, grouped by subsystem.
  const groups = useMemo(() => {
    if (!head) return [] as { name: string; cols: string[] }[];
    const q = colSearch.trim().toLowerCase();
    const byGroup = new Map<string, string[]>();
    for (const c of head.columns) {
      if (q && !c.toLowerCase().includes(q)) continue;
      const g = groupOf(c);
      (byGroup.get(g) ?? byGroup.set(g, []).get(g)!).push(c);
    }
    return [...byGroup.entries()].map(([name, cols]) => ({ name, cols }));
  }, [head, colSearch]);

  const toggle = (c: string) =>
    setSelected((prev) => {
      const next = new Set(prev);
      next.has(c) ? next.delete(c) : next.add(c);
      return next;
    });
  const setMany = (cols: string[], on: boolean) =>
    setSelected((prev) => {
      const next = new Set(prev);
      cols.forEach((c) => (on ? next.add(c) : next.delete(c)));
      return next;
    });

  // Selected columns in original file order, paired with their row index, so
  // the preview table is a flat lookup instead of an indexOf per cell.
  const orderedCols = useMemo(
    () => (head ? head.columns.map((name, index) => ({ name, index })).filter((c) => selected.has(c.name)) : []),
    [head, selected],
  );
  const orderedSelected = useMemo(() => orderedCols.map((c) => c.name), [orderedCols]);

  const downloadHref = (cols?: string[]) => {
    const base = `/api/logsync/jobs/${jobId}/files/${encodeURIComponent(fileName)}`;
    if (!cols || cols.length === 0) return base;
    return `${base}?cols=${encodeURIComponent(cols.join(','))}`;
  };

  const visibleCols = groups.flatMap((g) => g.cols);
  const allVisibleSelected = visibleCols.length > 0 && visibleCols.every((c) => selected.has(c));

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4" onMouseDown={onClose}>
      <div
        className="flex w-full max-w-4xl max-h-[90vh] flex-col rounded-xl border bg-card shadow-2xl"
        onMouseDown={(e) => e.stopPropagation()}
      >
        {/* header */}
        <div className="flex items-start justify-between gap-3 border-b p-5">
          <div className="min-w-0">
            <p className="font-mono text-sm truncate">{fileName}</p>
            <p className="text-xs text-muted-foreground mt-1">
              {subtitle ? `${subtitle} · ` : ''}
              {head ? `${head.total_columns} columns` : error ? 'failed to load' : 'loading…'}
            </p>
          </div>
          <button onClick={onClose} className="shrink-0 text-muted-foreground hover:text-foreground text-xl leading-none">×</button>
        </div>

        {error && <div className="p-5 text-sm text-red-400">Could not load preview: {error}</div>}
        {!head && !error && <div className="p-8 text-center text-sm text-muted-foreground">Loading preview…</div>}

        {head && (
          <div className="flex min-h-0 flex-1 flex-col md:flex-row">
            {/* column picker */}
            <div className="flex w-full md:w-80 shrink-0 flex-col border-b md:border-b-0 md:border-r">
              <div className="p-3 border-b flex flex-col gap-2">
                <Input placeholder="Search columns…" value={colSearch} onChange={(e) => setColSearch(e.target.value)} />
                <div className="flex items-center justify-between text-xs text-muted-foreground">
                  <span>{selected.size} selected</span>
                  <button className="underline hover:text-foreground" onClick={() => setMany(visibleCols, !allVisibleSelected)}>
                    {allVisibleSelected ? 'clear shown' : 'select shown'}
                  </button>
                </div>
              </div>
              <div className="overflow-y-auto max-h-[28rem] md:max-h-none flex-1 p-2">
                {groups.map((g) => {
                  const allOn = g.cols.every((c) => selected.has(c));
                  return (
                    <div key={g.name} className="mb-2">
                      <div className="flex items-center justify-between px-1 py-1">
                        <span className="text-xs font-semibold text-muted-foreground">{g.name}</span>
                        <button className="text-[10px] underline text-muted-foreground hover:text-foreground"
                                onClick={() => setMany(g.cols, !allOn)}>
                          {allOn ? 'none' : 'all'}
                        </button>
                      </div>
                      {g.cols.map((c) => (
                        <label key={c} className="flex items-center gap-2 px-1 py-0.5 text-xs rounded hover:bg-muted/50 cursor-pointer">
                          <input type="checkbox" checked={selected.has(c)} onChange={() => toggle(c)} className="shrink-0" />
                          <span className="font-mono truncate" title={c}>{c}</span>
                          {sample[c] !== undefined && sample[c] !== '' && (
                            <span className="ml-auto shrink-0 text-muted-foreground/70 tabular-nums truncate max-w-[6rem]" title={sample[c]}>
                              {sample[c]}
                            </span>
                          )}
                        </label>
                      ))}
                    </div>
                  );
                })}
                {groups.length === 0 && <p className="p-2 text-xs text-muted-foreground">No columns match “{colSearch}”.</p>}
              </div>
            </div>

            {/* preview table of the selected columns */}
            <div className="min-w-0 flex-1 overflow-auto p-3">
              <p className="text-xs text-muted-foreground mb-2">
                First {head.returned_rows} rows · {orderedSelected.length} of {head.total_columns} columns
              </p>
              {orderedSelected.length === 0 ? (
                <p className="text-sm text-muted-foreground p-4">Select columns to preview them.</p>
              ) : (
                <table className="text-xs border-collapse">
                  <thead>
                    <tr>
                      {orderedCols.map((c) => (
                        <th key={c.name} className="sticky top-0 bg-card border px-2 py-1 text-left font-mono whitespace-nowrap" title={c.name}>{c.name}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {head.rows.map((row, ri) => (
                      <tr key={ri}>
                        {orderedCols.map((c) => (
                          <td key={c.name} className="border px-2 py-1 whitespace-nowrap tabular-nums">
                            {row[c.index] ?? ''}
                          </td>
                        ))}
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>
          </div>
        )}

        {/* footer */}
        <div className="flex items-center justify-between gap-3 border-t p-4">
          <a href={downloadHref()} className="text-xs underline text-muted-foreground hover:text-foreground">
            Download full file
          </a>
          <a
            href={orderedSelected.length ? downloadHref(orderedSelected) : undefined}
            onClick={(e) => { if (!orderedSelected.length) { e.preventDefault(); toast.error('Select at least one column'); } }}
          >
            <Button disabled={!orderedSelected.length}>
              Download {orderedSelected.length || ''} column{orderedSelected.length === 1 ? '' : 's'}
            </Button>
          </a>
        </div>
      </div>
    </div>
  );
}
