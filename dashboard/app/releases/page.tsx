'use client';

import { useEffect, useState } from 'react';
import {
    fetchReleases, fetchWorkflowRuns,
    FIRMWARE_TARGETS, getTargetReleases, getGlobalReleases, getLatestRelease, type Release, type WorkflowRun,
} from '@/lib/github';
import FlashModal from '@/components/FlashModal';

export default function TargetMatrixPage() {
    const [releases, setReleases] = useState<Release[]>([]);
    const [runs, setRuns] = useState<WorkflowRun[]>([]);
    const [loading, setLoading] = useState(true);
    const [search, setSearch] = useState('');
    const [filter, setFilter] = useState('ALL');
    const [flashTarget, setFlashTarget] = useState<{ id: string; name: string; version: string; sha: string } | null>(null);

    useEffect(() => {
        Promise.all([fetchReleases(), fetchWorkflowRuns()])
            .then(([r, w]) => {
                setReleases(r);
                setRuns(w.workflow_runs);
                setLoading(false);
            })
            .catch(() => setLoading(false));
    }, []);

    // Build status from workflow runs
    const targetBuildStatus: Record<string, string> = {};
    for (const run of runs) {
        const isGlobal = ["presubmit", "postsubmit", "release"].some(w => run.name?.toLowerCase().includes(w));
        if (isGlobal && !targetBuildStatus['GLOBAL']) {
            targetBuildStatus['GLOBAL'] = run.conclusion || 'in_progress';
        }
        for (const t of FIRMWARE_TARGETS) {
            if (!targetBuildStatus[t.id]) {
                if (isGlobal || run.name?.toLowerCase().includes(t.id.toLowerCase())) {
                    targetBuildStatus[t.id] = run.conclusion || 'in_progress';
                }
            }
        }
    }

    function timeAgo(dateStr: string): string {
        const now = new Date();
        const date = new Date(dateStr);
        const diff = Math.floor((now.getTime() - date.getTime()) / 1000);
        if (diff < 60) return `${diff}s ago`;
        if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
        if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
        return `${Math.floor(diff / 86400)}d ago`;
    }

    const getIconForTarget = (id: string) => {
        if (id === 'GLOBAL') return 'public';
        const map: Record<string, string> = {
            VCU: 'memory', HVC: 'battery_charging_full', CSM: 'sensors',
            DUI: 'display_settings', LVBMS: 'battery_std', TSM: 'device_thermostat',
            USM: 'precision_manufacturing', PDU: 'electrical_services', BEVO: 'cell_tower',
        };
        return map[id] || 'memory';
    };

    const allTargets = [
        { id: 'GLOBAL', fullName: 'Global Trunk Release', chip: 'All Architectures' },
        ...FIRMWARE_TARGETS
    ];

    const filteredTargets = allTargets.filter(t => {
        if (filter !== 'ALL' && t.id !== filter) return false;
        if (search && !t.id.toLowerCase().includes(search.toLowerCase()) && !t.fullName.toLowerCase().includes(search.toLowerCase())) return false;
        return true;
    });

    const getLatestForTarget = (targetId: string) => {
        if (targetId === 'GLOBAL') {
            return getLatestRelease(getGlobalReleases(releases));
        }
        return getLatestRelease(getTargetReleases(releases, targetId));
    };

    return (
        <>
            {/* Header */}
            <header className="page-header-bar">
                <div className="page-header-left">
                    <h2 className="page-header-title">Target Matrix</h2>
                    <div style={{ height: 24, width: 1, background: 'var(--stroke)' }} />
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8, color: 'var(--text-muted)', fontFamily: 'var(--font-mono)', fontSize: 13 }}>
                        <span className="material-symbols-outlined" style={{ fontSize: 16 }}>cloud_done</span>
                        <span>FIRMWARE REPO: SYNCED</span>
                    </div>
                </div>
                <div className="page-header-right">
                    <div className="search-wrap">
                        <span className="material-symbols-outlined">search</span>
                        <input
                            type="text"
                            className="search-input"
                            placeholder="SEARCH TARGET ID..."
                            value={search}
                            onChange={e => setSearch(e.target.value)}
                        />
                    </div>
                    <button
                        className="btn-secondary"
                        onClick={() => { setLoading(true); Promise.all([fetchReleases(), fetchWorkflowRuns()]).then(([r, w]) => { setReleases(r); setRuns(w.workflow_runs); setLoading(false); }); }}
                    >
                        <span className="material-symbols-outlined" style={{ fontSize: 18 }}>refresh</span>
                        Refresh
                    </button>
                </div>
            </header>

            {/* Filters */}
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '24px 32px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                    <span style={{ fontFamily: 'var(--font-mono)', fontSize: 13, color: 'var(--text-muted)', textTransform: 'uppercase', marginRight: 8 }}>Filter Targets:</span>
                    <button className={`filter-pill ${filter === 'ALL' ? 'active' : ''}`} onClick={() => setFilter('ALL')}>All Systems</button>
                    {allTargets.map(t => (
                        <button key={t.id} className={`filter-pill ${filter === t.id ? 'active' : ''}`} onClick={() => setFilter(t.id)}>
                            {t.id}
                        </button>
                    ))}
                </div>
            </div>

            {/* Table */}
            <main style={{ flex: 1, overflowY: 'auto', padding: '0 32px 32px' }}>
                {/* Header */}
                <div className="matrix-header">
                    <div>Target Identity</div>
                    <div>Version Status</div>
                    <div>Build Metadata</div>
                    <div>Hardware</div>
                    <div style={{ textAlign: 'right' }}>Actions</div>
                </div>

                {loading ? (
                    <div className="loading-text">Loading target data…</div>
                ) : filteredTargets.length === 0 ? (
                    <div className="empty-state"><p>No targets match your search</p></div>
                ) : (
                    filteredTargets.map(target => {
                        const latestRelease = getLatestForTarget(target.id);
                        const latest = latestRelease ? {
                            version: latestRelease.version || latestRelease.tag_name,
                            sha: latestRelease.tag_name.substring(0, 6),
                            time: latestRelease.published_at,
                            author: latestRelease.author?.login || 'unknown',
                            assets: latestRelease.assets.filter(a => target.id === 'GLOBAL' || a.name.toLowerCase().includes(target.id.toLowerCase()))
                        } : null;

                        const locations = new Set<string>();
                        if (latest && target.id !== 'GLOBAL') {
                            latest.assets.forEach(a => {
                                const baseName = a.name.replace(/\.[^.]+$/, '');
                                const locMatch = baseName.match(/_([A-Z]{2})$/i);
                                if (locMatch) locations.add(locMatch[1].toUpperCase());
                            });
                        }
                        const locArray = Array.from(locations).sort();

                        const status = targetBuildStatus[target.id];
                        const isFailing = status === 'failure';
                        const versionBadgeCls = isFailing ? 'error' : (status === 'success' ? 'stable' : 'warning');
                        const versionLabel = isFailing ? 'BUILD FAILED' : (latest ? `${latest.version} ${status === 'success' ? 'STABLE' : 'UNKNOWN'}` : 'NO RELEASE');

                        return (
                            <div key={target.id} className="matrix-row">
                                {/* Identity */}
                                <div className="matrix-identity">
                                    <div className="matrix-icon">
                                        <span className="material-symbols-outlined">{getIconForTarget(target.id)}</span>
                                    </div>
                                    <div style={{ display: 'flex', flexDirection: 'column' }}>
                                        <span className="matrix-name">{target.id}</span>
                                        <span className="matrix-id">{target.fullName}</span>
                                    </div>
                                </div>
                                {/* Version */}
                                <div>
                                    <div className={`matrix-version-badge ${versionBadgeCls}`}>
                                        {!isFailing && <span className="matrix-version-dot" />}
                                        {isFailing && <span className="material-symbols-outlined" style={{ fontSize: 12 }}>block</span>}
                                        {versionLabel}
                                    </div>
                                </div>
                                {/* Metadata */}
                                <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                                    {latest ? (
                                        <>
                                            <div className="matrix-sha">
                                                <span className="material-symbols-outlined" style={{ fontSize: 16 }}>commit</span>
                                                <span className="matrix-sha-link">sha: {latest.sha}</span>
                                            </div>
                                            <span className="matrix-built-by">Built {timeAgo(latest.time)} by @{latest.author}</span>
                                        </>
                                    ) : (
                                        <span className="matrix-built-by">No releases yet</span>
                                    )}
                                </div>
                                {/* Hardware */}
                                <div>
                                    <span className="matrix-chip">{target.chip}</span>
                                </div>
                                {/* Actions */}
                                {/* Actions */}
                                <div className="matrix-actions">
                                    {target.id === 'GLOBAL' && latest ? (
                                        <div style={{ display: 'flex', flexDirection: 'column', gap: 8, width: '100%' }}>
                                            <a
                                                href={`https://github.com/LonghornRacingElectric/lhre-2026/releases/tag/${latest.version}`}
                                                target="_blank"
                                                rel="noopener noreferrer"
                                                className="btn-primary"
                                                style={{ textDecoration: 'none', textAlign: 'center' }}
                                            >
                                                <span className="material-symbols-outlined" style={{ fontSize: 18 }}>open_in_new</span>
                                                VIEW TRUNK
                                            </a>
                                        </div>
                                    ) : isFailing ? (
                                        <a
                                            href="https://github.com/LonghornRacingElectric/lhre-2026/actions"
                                            target="_blank"
                                            rel="noopener noreferrer"
                                            style={{
                                                display: 'flex', alignItems: 'center', gap: 8,
                                                borderRadius: 4, border: '1px solid rgba(218, 54, 51, 0.5)',
                                                color: 'var(--error)', padding: '8px 16px', fontSize: 13,
                                                fontWeight: 700, textDecoration: 'none', transition: 'all 0.2s',
                                            }}
                                        >
                                            <span className="material-symbols-outlined" style={{ fontSize: 18 }}>bug_report</span>
                                            LOGS
                                        </a>
                                    ) : latest ? (
                                        <div style={{ display: 'flex', flexDirection: 'column', gap: 8, width: '100%' }}>
                                            {(() => {
                                                const groups: Record<string, { loc: string | null; bin?: string; hex?: string; elf?: string }> = {};
                                                latest.assets.forEach(a => {
                                                    const extMatch = a.name.match(/\.(bin|hex|elf)$/i);
                                                    if (!extMatch) return;
                                                    const ext = extMatch[1].toLowerCase();

                                                    const baseName = a.name.replace(/\.[^.]+$/, '');
                                                    const locMatch = baseName.match(/_([A-Z]{2})$/i);
                                                    const loc = locMatch ? locMatch[1].toUpperCase() : null;
                                                    const key = loc || 'DEFAULT';

                                                    if (!groups[key]) groups[key] = { loc };
                                                    groups[key][ext as 'bin' | 'hex' | 'elf'] = a.browser_download_url;
                                                });

                                                const groupVals = Object.values(groups).sort((a, b) => (a.loc || '').localeCompare(b.loc || ''));
                                                if (groupVals.length === 0) {
                                                    return <span className="matrix-built-by">No flashable artifacts</span>;
                                                }

                                                return groupVals.map(g => (
                                                    <div key={g.loc || 'default'} style={{ display: 'flex', flexDirection: 'column', gap: 4, background: 'var(--bg-dark)', padding: 6, borderRadius: 6, border: '1px solid var(--stroke)' }}>
                                                        {g.loc && <span style={{ fontSize: 10, fontWeight: 700, color: 'var(--text-muted)', textAlign: 'center' }}>LOCATION: {g.loc}</span>}
                                                        {g.bin && (
                                                            <button
                                                                className="btn-primary"
                                                                style={{ padding: '4px 8px', fontSize: 11, width: '100%' }}
                                                                onClick={() => setFlashTarget({
                                                                    id: g.loc ? `${target.id}_${g.loc}` : target.id,
                                                                    name: g.loc ? `${target.fullName} (${g.loc})` : target.fullName,
                                                                    version: latest.version,
                                                                    sha: latest.sha,
                                                                })}
                                                            >
                                                                <span className="material-symbols-outlined" style={{ fontSize: 14 }}>flash_on</span>
                                                                {g.loc ? `FLASH ${g.loc}` : 'FLASH TARGET'}
                                                            </button>
                                                        )}
                                                        <div style={{ display: 'flex', gap: 4 }}>
                                                            {['bin', 'hex', 'elf'].map(e => {
                                                                const url = g[e as 'bin' | 'hex' | 'elf'];
                                                                return url ? (
                                                                    <a key={e} href={url} target="_blank" rel="noopener noreferrer" title={`Download ${e.toUpperCase()}`} style={{
                                                                        flex: 1, textAlign: 'center', padding: '4px 0', fontSize: 10, background: 'rgba(255,255,255,0.05)',
                                                                        color: 'white', textDecoration: 'none', borderRadius: 4, border: '1px solid rgba(255,255,255,0.1)',
                                                                        transition: 'background 0.2s', fontFamily: 'var(--font-mono)'
                                                                    }}>
                                                                        {e.toUpperCase()}
                                                                    </a>
                                                                ) : null;
                                                            })}
                                                        </div>
                                                    </div>
                                                ));
                                            })()}
                                        </div>
                                    ) : (
                                        <span className="matrix-built-by">N/A</span>
                                    )}
                                </div>
                            </div>
                        );
                    })
                )}
            </main>

            {/* Flash Modal */}
            {flashTarget && (
                <FlashModal
                    targetId={flashTarget.id}
                    targetName={flashTarget.name}
                    version={flashTarget.version}
                    sha={flashTarget.sha}
                    onClose={() => setFlashTarget(null)}
                />
            )}
        </>
    );
}
