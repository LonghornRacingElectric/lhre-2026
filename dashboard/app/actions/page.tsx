'use client';

import { useMemo } from 'react';
import useSWR from 'swr';
import Link from 'next/link';
import { formatDistanceToNow, format, subDays, startOfDay } from 'date-fns';
import { CheckCircle2, XCircle, AlertCircle, CircleDashed } from 'lucide-react';
import type { WorkflowRun } from '@/lib/github';

const fetcher = (url: string) => fetch(url).then(r => r.json());

interface CommitGroup {
    sha: string;
    branch: string;
    latestTime: number;
    runs: WorkflowRun[];
}

interface DayData {
    date: Date;
    dateKey: string;
    total: number;
    success: number;
    failed: number;
    running: number;
}

export default function ActionsPage() {
    const { data, error, isLoading } = useSWR<{ total_count: number; workflow_runs: WorkflowRun[] }>(
        '/api/actions?per_page=100',
        fetcher,
        { refreshInterval: 15000 }
    );

    // Build heatmap data for last 12 weeks
    const heatmapData = useMemo(() => {
        const days: DayData[] = [];
        const totalDays = 7 * 12; // 12 weeks
        const today = startOfDay(new Date());

        for (let i = totalDays - 1; i >= 0; i--) {
            const date = subDays(today, i);
            const dateKey = format(date, 'yyyy-MM-dd');
            days.push({ date, dateKey, total: 0, success: 0, failed: 0, running: 0 });
        }

        if (data?.workflow_runs) {
            for (const run of data.workflow_runs) {
                const runDate = format(new Date(run.updated_at), 'yyyy-MM-dd');
                const day = days.find(d => d.dateKey === runDate);
                if (day) {
                    day.total++;
                    if (run.status === 'in_progress' || run.status === 'queued') {
                        day.running++;
                    } else if (run.conclusion === 'success') {
                        day.success++;
                    } else if (run.conclusion === 'failure' || run.conclusion === 'startup_failure' || run.conclusion === 'timed_out') {
                        day.failed++;
                    }
                }
            }
        }

        return days;
    }, [data]);

    // Organize heatmap into columns (weeks), each column has 7 rows (Sun-Sat)
    const heatmapWeeks = useMemo(() => {
        const weeks: DayData[][] = [];
        let currentWeek: DayData[] = [];

        for (const day of heatmapData) {
            const dayOfWeek = day.date.getDay(); // 0=Sun
            if (dayOfWeek === 0 && currentWeek.length > 0) {
                weeks.push(currentWeek);
                currentWeek = [];
            }
            currentWeek.push(day);
        }
        if (currentWeek.length > 0) weeks.push(currentWeek);
        return weeks;
    }, [heatmapData]);

    function getCellColor(day: DayData): string {
        if (day.total === 0) return 'var(--bg-elevated)';
        if (day.running > 0) return 'var(--warning)';
        if (day.failed > 0) {
            // Mix: if all failed, full red; if some failed, lighter red
            const ratio = day.failed / day.total;
            if (ratio > 0.5) return 'var(--danger)';
            return '#c0392b80';
        }
        // All success — intensity based on count
        if (day.total >= 6) return '#10b981';
        if (day.total >= 4) return '#10b981cc';
        if (day.total >= 2) return '#10b98199';
        return '#10b98155';
    }

    function getStatusConfig(run: WorkflowRun) {
        if (run.status === 'in_progress' || run.status === 'queued') {
            return {
                icon: <CircleDashed className="animate-spin" size={14} />,
                label: run.status === 'in_progress' ? 'In Progress' : 'Queued',
                className: 'node-running'
            };
        }
        switch (run.conclusion) {
            case 'success':
                return { icon: <CheckCircle2 size={14} />, label: 'Success', className: 'node-success' };
            case 'failure':
            case 'startup_failure':
            case 'timed_out':
                return { icon: <XCircle size={14} />, label: 'Failed', className: 'node-failed' };
            case 'cancelled':
            case 'skipped':
            case 'neutral':
                return {
                    icon: <AlertCircle size={14} />,
                    label: run.conclusion.charAt(0).toUpperCase() + run.conclusion.slice(1),
                    className: 'node-neutral'
                };
            default:
                return { icon: <CircleDashed size={14} />, label: run.status, className: 'node-neutral' };
        }
    }

    function getGroupStatus(runs: WorkflowRun[]): string {
        if (runs.some(r => r.status === 'in_progress' || r.status === 'queued')) return 'node-running';
        if (runs.some(r => r.conclusion === 'failure' || r.conclusion === 'startup_failure' || r.conclusion === 'timed_out')) return 'node-failed';
        if (runs.every(r => r.conclusion === 'success')) return 'node-success';
        return 'node-neutral';
    }

    function getGroupIcon(cls: string) {
        switch (cls) {
            case 'node-running': return <CircleDashed className="animate-spin" size={18} />;
            case 'node-success': return <CheckCircle2 size={18} />;
            case 'node-failed': return <XCircle size={18} />;
            default: return <AlertCircle size={18} />;
        }
    }

    function buildGroups(runs: WorkflowRun[]): CommitGroup[] {
        const map = new Map<string, CommitGroup>();
        for (const run of runs) {
            const existing = map.get(run.head_sha);
            const t = new Date(run.updated_at).getTime();
            if (existing) {
                existing.runs.push(run);
                if (t > existing.latestTime) existing.latestTime = t;
            } else {
                map.set(run.head_sha, {
                    sha: run.head_sha,
                    branch: run.head_branch,
                    latestTime: t,
                    runs: [run],
                });
            }
        }
        return Array.from(map.values()).sort((a, b) => b.latestTime - a.latestTime);
    }

    if (error) {
        return (
            <div className="empty-state">
                <div className="empty-state-icon">❌</div>
                <h2>Failed to load actions</h2>
                <p>Please check your connection and try again.</p>
            </div>
        );
    }

    const groups = data ? buildGroups(data.workflow_runs) : [];
    const dayLabels = ['Sun', '', 'Tue', '', 'Thu', '', 'Sat'];

    return (
        <div className="actions-page animate-fade-in">
            <div className="page-header" style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between' }}>
                <div>
                    <div className="page-breadcrumb">
                        <Link href="/">Dashboard</Link>
                        <span>›</span>
                        <span style={{ color: 'var(--text-primary)' }}>Actions</span>
                    </div>
                    <h1 className="page-title">
                        Live GitHub Actions
                        <span className="badge warning" style={{ marginLeft: 12, verticalAlign: 'middle' }}>
                            <span className="badge-dot" style={{ animation: 'pulse 2s infinite' }}></span>
                            Live
                        </span>
                    </h1>
                    <p className="page-subtitle">
                        Real-time monitoring of CI/CD workflow runs. Auto-refreshes every 15s.
                    </p>
                </div>
            </div>

            {/* Contribution Graph */}
            <div className="section-card">
                <div className="section-header">
                    <h2 className="section-title">Workflow Activity</h2>
                    <div className="heatmap-legend">
                        <span style={{ fontSize: 11, color: 'var(--text-muted)', marginRight: 8 }}>Less</span>
                        <div className="heatmap-cell" style={{ background: 'var(--bg-elevated)' }}></div>
                        <div className="heatmap-cell" style={{ background: '#10b98155' }}></div>
                        <div className="heatmap-cell" style={{ background: '#10b98199' }}></div>
                        <div className="heatmap-cell" style={{ background: '#10b981cc' }}></div>
                        <div className="heatmap-cell" style={{ background: '#10b981' }}></div>
                        <span style={{ fontSize: 11, color: 'var(--text-muted)', marginLeft: 8 }}>More</span>
                    </div>
                </div>

                {isLoading ? (
                    <div style={{ padding: '20px 0', textAlign: 'center' }}>
                        <CircleDashed className="animate-spin" size={24} color="var(--accent)" />
                    </div>
                ) : (
                    <div className="heatmap-container">
                        <div className="heatmap-day-labels">
                            {dayLabels.map((label, i) => (
                                <span key={i} className="heatmap-day-label">{label}</span>
                            ))}
                        </div>
                        <div className="heatmap-grid">
                            {heatmapWeeks.map((week, wi) => (
                                <div className="heatmap-week" key={wi}>
                                    {week.map(day => (
                                        <div
                                            key={day.dateKey}
                                            className="heatmap-cell"
                                            style={{ background: getCellColor(day) }}
                                            title={`${format(day.date, 'MMM d, yyyy')}: ${day.total} run${day.total !== 1 ? 's' : ''} (${day.success} passed, ${day.failed} failed${day.running ? `, ${day.running} running` : ''})`}
                                        />
                                    ))}
                                </div>
                            ))}
                        </div>
                    </div>
                )}
            </div>

            {/* Recent Commits */}
            <div className="section-card">
                <div className="section-header">
                    <h2 className="section-title">Recent Commits</h2>
                    {data && (
                        <span style={{ fontSize: 13, color: 'var(--text-muted)' }}>
                            {groups.length} commits · {data.workflow_runs.length} runs
                        </span>
                    )}
                </div>

                {isLoading ? (
                    <div className="loading-state" style={{ padding: '40px 0', textAlign: 'center' }}>
                        <CircleDashed className="animate-spin" size={32} color="var(--accent)" style={{ margin: '0 auto 16px' }} />
                        <div className="loading-text">Fetching live workflow runs…</div>
                    </div>
                ) : groups.length === 0 ? (
                    <div className="empty-state">
                        <div className="empty-state-icon">⚡</div>
                        <p>No workflow runs found.</p>
                    </div>
                ) : (
                    <div className="workflows-list">
                        {groups.map((group, gi) => {
                            const groupCls = getGroupStatus(group.runs);
                            return (
                                <div className="commit-group" key={group.sha} style={{ animationDelay: `${0.04 * Math.min(gi, 15)}s` }}>
                                    <div className="workflow-status-col">
                                        <div className={`workflow-node ${groupCls}`} title={group.sha}>
                                            {getGroupIcon(groupCls)}
                                        </div>
                                        {gi < groups.length - 1 && <div className="workflow-connector"></div>}
                                    </div>

                                    <div className="commit-group-content">
                                        <div className="commit-group-header">
                                            <div>
                                                <code className="commit-hash">{group.sha.substring(0, 7)}</code>
                                                <span style={{ margin: '0 8px', color: 'var(--border-color)' }}>on</span>
                                                <strong style={{ color: 'var(--text-primary)' }}>{group.branch}</strong>
                                            </div>
                                            <span className="workflow-time" title={new Date(group.latestTime).toLocaleString()}>
                                                {formatDistanceToNow(new Date(group.latestTime), { addSuffix: true })}
                                            </span>
                                        </div>

                                        <div className="commit-runs">
                                            {group.runs.map(run => {
                                                const cfg = getStatusConfig(run);
                                                return (
                                                    <a
                                                        key={run.id}
                                                        href={run.html_url}
                                                        target="_blank"
                                                        rel="noopener noreferrer"
                                                        className={`commit-run-chip ${cfg.className}`}
                                                        title={`${run.name} — ${cfg.label}`}
                                                    >
                                                        {cfg.icon}
                                                        <span className="chip-label">{run.name}</span>
                                                    </a>
                                                );
                                            })}
                                        </div>
                                    </div>
                                </div>
                            );
                        })}
                    </div>
                )}
            </div>
        </div>
    );
}
