'use client';

import { use, useEffect, useState } from 'react';
import Link from 'next/link';
import { notFound } from 'next/navigation';
import {
    fetchReleases, getTargetReleases, FIRMWARE_TARGETS, type Release,
} from '@/lib/github';
import FlashModal from '@/components/FlashModal';

export default function TargetPage({ params }: { params: Promise<{ target: string }> }) {
    const resolvedParams = use(params);
    const targetId = resolvedParams.target.toUpperCase();
    const target = FIRMWARE_TARGETS.find(t => t.id === targetId);

    const [releases, setReleases] = useState<Release[]>([]);
    const [loading, setLoading] = useState(true);
    const [flashTarget, setFlashTarget] = useState<{ id: string; name: string; version: string; sha: string } | null>(null);

    if (!target) {
        notFound();
    }

    // eslint-disable-next-line react-hooks/rules-of-hooks
    useEffect(() => {
        fetchReleases()
            .then(allReleases => {
                setReleases(getTargetReleases(allReleases, targetId));
                setLoading(false);
            })
            .catch(() => setLoading(false));
    }, [targetId]);

    const latest = releases.length > 0 ? releases[0] : null;

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
            hour: '2-digit', minute: '2-digit'
        });
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            {/* Breadcrumbs */}
            <div className="breadcrumbs">
                <Link href="/releases">TARGET MATRIX</Link>
                <span className="sep">/</span>
                <span className="current">{targetId}_TELEMETRY</span>
            </div>

            <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
                {/* Left Column (Details & Specs) */}
                <div className="hide-scrollbar" style={{
                    minWidth: 400, width: '30%', padding: 24, paddingRight: 0,
                    display: 'flex', flexDirection: 'column', gap: 24, overflowY: 'auto'
                }}>
                    <div className="target-identity">
                        <div className="target-identity-label">
                            {targetId} IDENTITY
                            <span className="material-symbols-outlined" style={{ color: 'var(--text-muted)' }}>memory</span>
                        </div>
                        <h1 className="target-identity-name">{target.fullName}</h1>
                        <div className="target-identity-meta">
                            <span style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                                <span className="material-symbols-outlined" style={{ fontSize: 16 }}>corporate_fare</span>
                                Longhorn Racing Electric
                            </span>
                        </div>
                    </div>

                    <div className="live-box">
                        <div className="live-box-corner-tr" />
                        <div className="live-box-corner-bl" />
                        <div className="live-box-header">
                            <div className="live-box-title">
                                <span className="material-symbols-outlined" style={{ color: 'var(--primary)', animation: 'pulse 2s infinite' }}>sensors</span>
                                LIVE STATUS
                            </div>
                            <div className="live-box-active-badge">ACTIVE</div>
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
                            <div>
                                <div className="spec-label">ACTIVE PAYLOAD VERSION</div>
                                <div className="spec-value-big">{latest ? latest.version || latest.tag_name : 'No Release'}</div>
                            </div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
                                <div>
                                    <div className="spec-label">BUILD HASH</div>
                                    <div style={{ fontFamily: 'var(--font-mono)', fontSize: 14, color: 'white' }}>
                                        {latest ? latest.tag_name.substring(0, 6) : '------'}
                                    </div>
                                </div>
                                <div>
                                    <div className="spec-label">UPTIME</div>
                                    <div style={{ fontFamily: 'var(--font-mono)', fontSize: 14, color: 'white' }}>
                                        00:00:00 <span style={{ color: 'var(--text-muted)' }}>(STATIC)</span>
                                    </div>
                                </div>
                            </div>
                            <div>
                                <div className="spec-label">HEARTBEAT</div>
                                <svg width="100%" height="40" viewBox="0 0 300 40" fill="none" style={{ marginTop: 8 }}>
                                    <path d="M0 20 L50 20 L60 5 L70 35 L80 20 L300 20" stroke="var(--success)" strokeWidth="2" />
                                </svg>
                            </div>
                        </div>
                    </div>

                    <div className="specs-box">
                        <h2 className="specs-box-title">SPECIFICATIONS</h2>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                            <div className="spec-row">
                                <span className="spec-row-label">MCU Architecture</span>
                                <span className="spec-row-value">{target.chip}</span>
                            </div>
                            <div className="spec-row">
                                <span className="spec-row-label">Flash Memory</span>
                                <span className="spec-row-value">512 KB</span>
                            </div>
                            <div className="spec-row">
                                <span className="spec-row-label">SRAM</span>
                                <span className="spec-row-value">128 KB</span>
                            </div>
                            <div className="spec-row">
                                <span className="spec-row-label">Core Clock</span>
                                <span className="spec-row-value">170 MHz</span>
                            </div>
                            <div className="spec-row">
                                <span className="spec-row-label">Compiler</span>
                                <span className="spec-row-value">GCC ARM 13.2.1</span>
                            </div>
                        </div>
                    </div>
                </div>

                {/* Right Column (Timeline) */}
                <div style={{ flex: 1, padding: 24, display: 'flex', flexDirection: 'column' }}>
                    <div className="timeline-panel">
                        <div className="timeline-header">
                            <div className="timeline-header-title">
                                <span className="material-symbols-outlined">hub</span>
                                RELEASE LINEAGE
                            </div>
                            <div style={{ fontFamily: 'var(--font-mono)', fontSize: 13, color: 'var(--primary)' }}>
                                {loading ? 'SYNCING...' : `${releases.length} EVENTS LOCATED`}
                            </div>
                        </div>
                        <div className="timeline-body">
                            <div className="timeline-line" />
                            {loading ? (
                                <div className="loading-text">Loading release history…</div>
                            ) : releases.length === 0 ? (
                                <div className="empty-state">
                                    <span className="material-symbols-outlined" style={{ fontSize: 40, marginBottom: 16 }}>history</span>
                                    <p>No releases found for this target.</p>
                                </div>
                            ) : (
                                releases.map((release, i) => {
                                    // Filter assets for this specific target
                                    const targetAssets = release.assets.filter(a => a.name.toLowerCase().includes(targetId.toLowerCase()));

                                    const isGlobal = !release.targetPrefix;
                                    const scopeBadgeCls = isGlobal ? 'stable' : 'warning';
                                    const scopeLabel = isGlobal ? 'GLOBAL TRUNK' : 'TARGET BUILD';

                                    const isLatest = i === 0;
                                    const badgeCls = release.draft ? 'muted' : release.prerelease ? 'warning' : 'stable';
                                    const badgeLabel = release.draft ? 'DRAFT' : release.prerelease ? 'PRE-RELEASE' : 'STABLE';
                                    return (
                                        <div key={release.id} className="timeline-item">
                                            {isLatest ? (
                                                <div className="timeline-active-dot">
                                                    <div className="timeline-active-dot-outer" />
                                                    <div className="timeline-active-dot-inner" />
                                                </div>
                                            ) : (
                                                <div className={`timeline-dot ${release.prerelease ? 'warning' : ''}`} />
                                            )}
                                            <div className={`timeline-card ${isLatest ? 'active' : ''} ${release.prerelease ? 'warning-border' : ''} ${i > 1 ? 'faded' : ''}`}>
                                                <div className="timeline-card-header">
                                                    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                                                        <div className="timeline-card-title" style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                                                            <span className={`timeline-card-version ${isLatest ? 'success' : ''}`}>
                                                                {release.version || release.tag_name}
                                                            </span>
                                                            {release.name && (
                                                                <span style={{ fontSize: 13, color: 'var(--text)', fontWeight: 600 }}>
                                                                    {release.name}
                                                                </span>
                                                            )}
                                                            <div style={{ display: 'flex', gap: 4, marginLeft: 'auto' }}>
                                                                <span className={`timeline-card-tag ${scopeBadgeCls}`}>{scopeLabel}</span>
                                                                <span className={`timeline-card-tag ${badgeCls}`}>{badgeLabel}</span>
                                                            </div>
                                                        </div>
                                                        <div className="timeline-card-time">{formatDate(release.published_at)} ({timeAgo(release.published_at)})</div>
                                                    </div>
                                                    <a href={release.html_url} target="_blank" rel="noopener noreferrer" className="btn-icon">
                                                        <span className="material-symbols-outlined">exit_to_app</span>
                                                    </a>
                                                </div>
                                                <div className={`timeline-card-body ${isLatest ? 'white' : ''}`} style={{ whiteSpace: 'normal', display: '-webkit-box', WebkitLineClamp: 3, WebkitBoxOrient: 'vertical' }}>
                                                    {release.body || 'No release notes provided.'}
                                                </div>
                                                <div className="timeline-card-footer">
                                                    <div className="timeline-card-meta">
                                                        <span className="material-symbols-outlined">account_circle</span>
                                                        <span>@{release.author?.login || 'unknown'}</span>
                                                    </div>
                                                    <div className="timeline-card-meta" style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-start', gap: 4 }}>
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                                                            <span className="material-symbols-outlined">folder_zip</span>
                                                            <span>{targetAssets.length} flashable artifacts</span>
                                                        </div>
                                                        {targetAssets.length > 0 && (
                                                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 8, marginTop: 4 }}>
                                                                {(() => {
                                                                    const groups: Record<string, { loc: string | null; bin?: string; hex?: string; elf?: string }> = {};
                                                                    targetAssets.forEach(a => {
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

                                                                    return Object.values(groups).sort((a, b) => (a.loc || '').localeCompare(b.loc || '')).map(g => (
                                                                        <div key={g.loc || 'default'} style={{ display: 'flex', flexDirection: 'column', gap: 4, background: 'var(--bg-dark)', padding: 6, borderRadius: 6, border: '1px solid var(--stroke)', minWidth: 140 }}>
                                                                            {g.loc && <span style={{ fontSize: 10, fontWeight: 700, color: 'var(--text-muted)', textAlign: 'center' }}>LOCATION: {g.loc}</span>}
                                                                            {g.bin && (
                                                                                <button
                                                                                    className="btn-primary"
                                                                                    style={{ padding: '4px 8px', fontSize: 11, width: '100%' }}
                                                                                    onClick={() => setFlashTarget({
                                                                                        id: g.loc ? `${target.id}_${g.loc}` : target.id,
                                                                                        name: g.loc ? `${target.fullName} (${g.loc})` : target.fullName,
                                                                                        version: release.version || release.tag_name,
                                                                                        sha: release.tag_name.substring(0, 6)
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
                                                        )}
                                                    </div>
                                                </div>
                                            </div>
                                        </div>
                                    );
                                })
                            )}
                        </div>
                    </div>
                </div>
            </div>

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
        </div>
    );
}
