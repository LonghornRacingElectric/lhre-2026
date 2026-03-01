'use client';

import { useEffect, useState } from 'react';
import { useParams } from 'next/navigation';
import Link from 'next/link';
import {
    fetchReleases, fetchWorkflowRuns, getTargetReleases,
    FIRMWARE_TARGETS, type Release, type WorkflowRun,
} from '@/lib/github';

export default function TargetPage() {
    const params = useParams();
    const targetId = params.target as string;
    const target = FIRMWARE_TARGETS.find(t => t.id === targetId);

    const [releases, setReleases] = useState<Release[]>([]);
    const [allRuns, setAllRuns] = useState<WorkflowRun[]>([]);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        Promise.all([fetchReleases(), fetchWorkflowRuns()])
            .then(([r, w]) => {
                setReleases(r);
                setAllRuns(w.workflow_runs);
                setLoading(false);
            })
            .catch(() => setLoading(false));
    }, []);

    if (!target) {
        return (
            <div className="empty-state">
                <div className="empty-state-icon">❓</div>
                <p>Target &quot;{targetId}&quot; not found.</p>
                <Link href="/" className="btn btn-secondary" style={{ marginTop: 16 }}>Back to Dashboard</Link>
            </div>
        );
    }

    const targetReleases = getTargetReleases(releases, targetId)
        .sort((a, b) => new Date(b.published_at).getTime() - new Date(a.published_at).getTime());
    const latestRelease = targetReleases[0];

    // Filter runs that might relate to this target
    const runs = allRuns.slice(0, 20);
    const successRuns = runs.filter(r => r.conclusion === 'success').length;
    const totalRuns = runs.length;
    const successRate = totalRuns > 0 ? ((successRuns / totalRuns) * 100).toFixed(1) : '—';

    function timeAgo(dateStr: string): string {
        const now = new Date();
        const date = new Date(dateStr);
        const diff = Math.floor((now.getTime() - date.getTime()) / 1000);
        if (diff < 60) return `${diff}s ago`;
        if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
        if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
        return `${Math.floor(diff / 86400)}d ago`;
    }

    function formatDate(dateStr: string): string {
        return new Date(dateStr).toLocaleDateString('en-US', {
            year: 'numeric', month: 'short', day: 'numeric',
        });
    }

    return (
        <>
            {/* Breadcrumb */}
            <div className="page-header">
                <div className="page-breadcrumb">
                    <Link href="/">Dashboard</Link>
                    <span>›</span>
                    <Link href="/">Targets</Link>
                    <span>›</span>
                    <span style={{ color: 'var(--text-primary)' }}>{target.name}</span>
                </div>
                <h1 className="page-title">{target.fullName}</h1>
                <p className="page-subtitle">{target.description} Managed via Bazel build system.</p>
            </div>

            {/* Stats */}
            <div className="stats-grid">
                <div className="stat-card">
                    <div className="stat-label"><span className="stat-dot blue" /> Total Builds</div>
                    <div className="stat-value">{loading ? '…' : totalRuns}</div>
                    <div className="stat-trend neutral">Recent runs</div>
                </div>
                <div className="stat-card">
                    <div className="stat-label"><span className="stat-dot green" /> Success Rate</div>
                    <div className="stat-value">{loading ? '…' : `${successRate}%`}</div>
                    <div className="stat-trend up">
                        {successRuns}/{totalRuns} passing
                    </div>
                </div>
                <div className="stat-card">
                    <div className="stat-label"><span className="stat-dot orange" /> Bazel Target</div>
                    <div className="stat-value" style={{ fontSize: 14, fontFamily: 'monospace', color: 'var(--accent-light)' }}>
                        {target.bazelTarget}
                    </div>
                    <div className="stat-trend neutral">{target.chip}</div>
                </div>
                <div className="stat-card">
                    <div className="stat-label"><span className="stat-dot purple" /> Latest Release</div>
                    <div className="stat-value" style={{ fontSize: latestRelease ? 22 : 28 }}>
                        {loading ? '…' : latestRelease ? latestRelease.version || latestRelease.tag_name : 'None'}
                    </div>
                    <div className="stat-trend neutral">
                        {latestRelease ? timeAgo(latestRelease.published_at) : ''}
                    </div>
                </div>
            </div>

            {/* Two Column: Build History + Target Releases */}
            <div className="two-col">
                {/* Build History */}
                <div className="section-card" style={{ animationDelay: '0.2s' }}>
                    <div className="section-header">
                        <h2 className="section-title">Build History</h2>
                        <a
                            href={`https://github.com/LonghornRacingElectric/lhre-2026/actions`}
                            target="_blank"
                            rel="noopener noreferrer"
                            className="section-action"
                            style={{ textDecoration: 'none' }}
                        >
                            View All Build History
                        </a>
                    </div>
                    {loading ? (
                        <div className="loading-text">Loading…</div>
                    ) : runs.length === 0 ? (
                        <div className="empty-state">
                            <div className="empty-state-icon">🔨</div>
                            <p>No build runs found</p>
                        </div>
                    ) : (
                        <table className="build-table">
                            <thead>
                                <tr>
                                    <th>Status</th>
                                    <th>Build</th>
                                    <th>Commit</th>
                                    <th>Branch</th>
                                    <th>Time</th>
                                </tr>
                            </thead>
                            <tbody>
                                {runs.slice(0, 8).map(run => (
                                    <tr key={run.id}>
                                        <td>
                                            <span className={`badge ${run.conclusion === 'success' ? 'success' : run.conclusion === 'failure' ? 'failed' : run.conclusion === 'cancelled' ? 'warning' : 'info'}`}>
                                                <span className="badge-dot" />
                                                {run.conclusion === 'success' ? 'Success' : run.conclusion === 'failure' ? 'Failed' : run.conclusion || 'Running'}
                                            </span>
                                        </td>
                                        <td>
                                            <a href={run.html_url} target="_blank" rel="noopener noreferrer">
                                                #{run.run_number}
                                            </a>
                                        </td>
                                        <td>
                                            <span className="commit-hash">{run.head_sha.substring(0, 7)}</span>
                                        </td>
                                        <td>{run.head_branch}</td>
                                        <td style={{ color: 'var(--text-muted)' }}>{timeAgo(run.created_at)}</td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    )}
                </div>

                {/* Target Releases */}
                <div className="section-card" style={{ animationDelay: '0.3s' }}>
                    <div className="section-header">
                        <h2 className="section-title">Target Releases</h2>
                    </div>
                    {loading ? (
                        <div className="loading-text">Loading…</div>
                    ) : targetReleases.length === 0 ? (
                        <div className="empty-state">
                            <div className="empty-state-icon">📦</div>
                            <p>No releases for {target.name} yet</p>
                            <p style={{ fontSize: 12, marginTop: 8, color: 'var(--text-muted)' }}>
                                Create a tag like <code style={{ color: 'var(--accent-light)' }}>{target.id}/firmware/v1.0.0</code> to publish a release.
                            </p>
                        </div>
                    ) : (
                        targetReleases.map(r => (
                            <div className="release-item" key={r.id}>
                                <div className="release-item-header">
                                    <div>
                                        <div className="release-item-version">
                                            {r.version || r.tag_name}
                                            {' '}
                                            {r.prerelease && <span className="badge warning" style={{ marginLeft: 8 }}>Pre-release</span>}
                                        </div>
                                        <div className="release-item-date">Released {formatDate(r.published_at)}</div>
                                    </div>
                                    {r.author && (
                                        <div className="author">
                                            {r.author.avatar_url && (
                                                // eslint-disable-next-line @next/next/no-img-element
                                                <img src={r.author.avatar_url} alt={r.author.login} className="author-avatar" />
                                            )}
                                            <span className="author-name">{r.author.login}</span>
                                        </div>
                                    )}
                                </div>
                                {r.body && (
                                    <div className="release-item-body">
                                        {r.body.split('\n').map((line, i) => (
                                            <span key={i}>{line}<br /></span>
                                        ))}
                                    </div>
                                )}
                                {r.assets.length > 0 && (
                                    <div className="release-assets">
                                        {r.assets.map(a => (
                                            <a key={a.name} href={a.browser_download_url} className="release-asset" target="_blank" rel="noopener noreferrer">
                                                📎 {a.name}
                                            </a>
                                        ))}
                                    </div>
                                )}
                                <div style={{ marginTop: 12 }}>
                                    <a href={r.html_url} target="_blank" rel="noopener noreferrer" className="btn btn-primary" style={{ fontSize: 12, padding: '6px 14px' }}>
                                        ⬇ Download
                                    </a>
                                    <a href={r.html_url} target="_blank" rel="noopener noreferrer" className="btn btn-secondary" style={{ fontSize: 12, padding: '6px 14px', marginLeft: 8 }}>
                                        📝 Release Notes
                                    </a>
                                </div>
                            </div>
                        ))
                    )}
                </div>
            </div>

            <footer className="footer">
                <span>© 2026 Longhorn Racing Electric. Built for the track.</span>
                <div className="footer-links">
                    <a href="https://github.com/LonghornRacingElectric/lhre-2026" target="_blank" rel="noopener noreferrer">GitHub</a>
                    <a href="https://github.com/LonghornRacingElectric/lhre-2026/actions" target="_blank" rel="noopener noreferrer">CI/CD</a>
                </div>
            </footer>
        </>
    );
}
