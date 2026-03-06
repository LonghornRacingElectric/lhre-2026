'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import ReactMarkdown from 'react-markdown';
import {
    fetchReleases, getGlobalReleases, getLatestRelease,
    FIRMWARE_TARGETS, type Release,
} from '@/lib/github';

export default function ReleasesPage() {
    const [releases, setReleases] = useState<Release[]>([]);
    const [loading, setLoading] = useState(true);
    const [search, setSearch] = useState('');

    useEffect(() => {
        fetchReleases()
            .then(r => {
                setReleases(r);
                setLoading(false);
            })
            .catch(() => setLoading(false));
    }, []);

    const allReleases = [...releases]
        .sort((a, b) => new Date(b.published_at).getTime() - new Date(a.published_at).getTime());

    const globalReleases = getGlobalReleases(releases)
        .sort((a, b) => new Date(b.published_at).getTime() - new Date(a.published_at).getTime());

    const latestStable = getLatestRelease(globalReleases.filter(r => !r.prerelease && !r.draft));

    const filteredReleases = search
        ? allReleases.filter(r =>
            r.tag_name.toLowerCase().includes(search.toLowerCase()) ||
            r.name?.toLowerCase().includes(search.toLowerCase()) ||
            r.body?.toLowerCase().includes(search.toLowerCase())
        )
        : allReleases;

    function formatDate(dateStr: string): string {
        return new Date(dateStr).toLocaleDateString('en-US', {
            year: 'numeric', month: 'short', day: 'numeric',
        });
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

    function getBadgeClass(r: Release): string {
        if (r.draft) return 'neutral';
        if (r.prerelease) return 'warning';
        return 'success';
    }

    function getBadgeLabel(r: Release): string {
        if (r.draft) return 'Draft';
        if (r.prerelease) return 'Pre-release';
        return 'Stable';
    }

    function extractLocation(assetName: string, targetId: string): string | null {
        // Asset names follow pattern: {target}_firmware_2026_{LOCATION}.{ext}
        // e.g. csm_firmware_2026_FL.bin -> "FL"
        const baseName = assetName.replace(/\.[^.]+$/, ''); // strip extension
        const targetLower = targetId.toLowerCase();
        const lowerBase = baseName.toLowerCase();

        // Find the target id in the name and check what comes after
        const idx = lowerBase.indexOf(targetLower);
        if (idx === -1) return null;

        const afterTarget = baseName.substring(idx + targetId.length);
        // Match pattern like _firmware_2026_FL or just ending with _FL
        const locMatch = afterTarget.match(/_([A-Z]{2})$/i);
        if (locMatch) {
            return locMatch[1].toUpperCase();
        }
        return null;
    }

    const LOCATION_LABELS: Record<string, string> = {
        FL: 'Front Left',
        FR: 'Front Right',
        RL: 'Rear Left',
        RR: 'Rear Right',
    };

    function renderAssetsGroups(assets?: { name: string; browser_download_url: string }[]) {
        if (!assets || assets.length === 0) return null;

        const grouped: Record<string, typeof assets> = {};
        const others: typeof assets = [];

        assets.forEach(asset => {
            const lowerName = asset.name.toLowerCase();
            const target = FIRMWARE_TARGETS.find(t => lowerName.includes(t.id.toLowerCase()));
            if (target) {
                if (!grouped[target.id]) grouped[target.id] = [];
                grouped[target.id].push(asset);
            } else {
                others.push(asset);
            }
        });

        return (
            <div className="release-assets-grouped">
                {FIRMWARE_TARGETS.map(target => {
                    const targetAssets = grouped[target.id];
                    if (!targetAssets || targetAssets.length === 0) return null;

                    // Sub-group by location
                    const byLocation: Record<string, typeof targetAssets> = {};
                    const noLocation: typeof targetAssets = [];

                    targetAssets.forEach(a => {
                        const loc = extractLocation(a.name, target.id);
                        if (loc) {
                            if (!byLocation[loc]) byLocation[loc] = [];
                            byLocation[loc].push(a);
                        } else {
                            noLocation.push(a);
                        }
                    });

                    const locations = Object.keys(byLocation).sort();
                    const hasLocations = locations.length > 0;

                    return (
                        <div key={target.id} className="release-target-assets">
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                                <div className={`target-icon small ${target.id.toLowerCase()}`} title={target.fullName}>
                                    {target.id.substring(0, 3)}
                                </div>
                                <span style={{ fontSize: 13, fontWeight: 600, color: 'var(--text-primary)' }}>{target.name}</span>
                                {hasLocations && (
                                    <span style={{ fontSize: 11, color: 'var(--text-muted)', fontWeight: 400 }}>
                                        {locations.length} location{locations.length !== 1 ? 's' : ''}
                                    </span>
                                )}
                            </div>
                            {!hasLocations ? (
                                /* Single location - flat download links */
                                <div className="target-asset-links" style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center' }}>
                                    <span className="target-asset-label">Downloads:</span>
                                    {noLocation.map(a => {
                                        const ext = a.name.split('.').pop()?.toUpperCase() || 'FILE';
                                        return (
                                            <a key={a.name} href={a.browser_download_url} className="target-asset-link" target="_blank" rel="noopener noreferrer">
                                                ⬇ {ext}
                                            </a>
                                        );
                                    })}
                                    <button className="target-asset-link ota-btn" onClick={() => { }} title="Over-the-Air Update (Coming Soon)">
                                        ☁️ OTA
                                    </button>
                                </div>
                            ) : (
                                /* Multiple locations - show sub-rows */
                                <div className="target-locations" style={{ width: '100%', marginTop: 6 }}>
                                    {locations.map(loc => (
                                        <div key={loc} className="target-location-row" style={{
                                            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                                            padding: '4px 0 4px 32px', borderTop: '1px solid var(--border-color)',
                                        }}>
                                            <span style={{ fontSize: 12, fontWeight: 500, color: 'var(--text-secondary)', minWidth: 80 }}>
                                                📍 {loc} <span style={{ color: 'var(--text-muted)', fontWeight: 400 }}>{LOCATION_LABELS[loc] || ''}</span>
                                            </span>
                                            <div className="target-asset-links" style={{ display: 'flex', alignItems: 'center' }}>
                                                {byLocation[loc].map(a => {
                                                    const ext = a.name.split('.').pop()?.toUpperCase() || 'FILE';
                                                    return (
                                                        <a key={a.name} href={a.browser_download_url} className="target-asset-link" target="_blank" rel="noopener noreferrer">
                                                            ⬇ {ext}
                                                        </a>
                                                    );
                                                })}
                                                <button className="target-asset-link ota-btn" onClick={() => { }} title={`OTA to ${loc} (Coming Soon)`}>
                                                    ☁️ OTA
                                                </button>
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            )}
                        </div>
                    );
                })}
                {others.length > 0 && (
                    <div className="release-target-assets">
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                            <div className="target-icon small" style={{ background: 'var(--border-color)', color: 'var(--text-muted)' }} title="Other Assets">
                                OTH
                            </div>
                            <span style={{ fontSize: 13, fontWeight: 600, color: 'var(--text-primary)' }}>Other</span>
                        </div>
                        <div className="target-asset-links" style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center' }}>
                            <span className="target-asset-label">Downloads:</span>
                            {others.map(a => (
                                <a key={a.name} href={a.browser_download_url} className="target-asset-link" target="_blank" rel="noopener noreferrer">
                                    ⬇ {a.name}
                                </a>
                            ))}
                        </div>
                    </div>
                )}
            </div>
        );
    }

    return (
        <>
            {/* Header */}
            <div className="page-header" style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between' }}>
                <div>
                    <div className="page-breadcrumb">
                        <Link href="/">Dashboard</Link>
                        <span>›</span>
                        <span style={{ color: 'var(--text-primary)' }}>Releases</span>
                    </div>
                    <h1 className="page-title">
                        Global Repository Releases
                        <span className="badge info" style={{ marginLeft: 12, verticalAlign: 'middle' }}>
                            lhre-2026
                        </span>
                    </h1>
                </div>
                <div className="search-bar" style={{ marginTop: 16 }}>
                    <span className="search-icon">🔍</span>
                    <input
                        type="text"
                        placeholder="Search releases..."
                        value={search}
                        onChange={e => setSearch(e.target.value)}
                    />
                </div>
            </div>

            {/* Featured Stable Release */}
            {!loading && latestStable && (
                <div className="release-featured">
                    <div className="release-featured-label">
                        <span className="star">⭐</span> Stable Trunk Release
                    </div>
                    <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between' }}>
                        <div style={{ position: 'relative', zIndex: 1 }}>
                            <div className="release-version">
                                {latestStable.version || latestStable.tag_name}
                                {' '}
                                <span className="badge success" style={{ verticalAlign: 'middle' }}>Production Ready</span>
                            </div>
                            <div className="release-date">
                                Released {formatDate(latestStable.published_at)} · {timeAgo(latestStable.published_at)}
                            </div>
                            {latestStable.body && (
                                <div className="release-body release-markdown">
                                    <ReactMarkdown>
                                        {latestStable.body}
                                    </ReactMarkdown>
                                </div>
                            )}
                            {latestStable.author && (
                                <div className="author" style={{ marginTop: 8 }}>
                                    {latestStable.author.avatar_url && (
                                        // eslint-disable-next-line @next/next/no-img-element
                                        <img src={latestStable.author.avatar_url} alt={latestStable.author.login} className="author-avatar" />
                                    )}
                                    <span className="author-name">by {latestStable.author.login}</span>
                                </div>
                            )}
                            <div className="release-actions" style={{ marginBottom: 16 }}>
                                <a href={latestStable.html_url} target="_blank" rel="noopener noreferrer" className="btn btn-secondary" style={{ padding: '8px 16px', fontSize: 12 }}>
                                    📝 View Release on GitHub
                                </a>
                            </div>
                            {renderAssetsGroups(latestStable.assets)}
                        </div>
                    </div>
                </div>
            )}

            {/* Release History */}
            <div className="section-header">
                <h2 className="section-title">Release History</h2>
                <span style={{ fontSize: 13, color: 'var(--text-muted)' }}>
                    {filteredReleases.length} release{filteredReleases.length !== 1 ? 's' : ''}
                </span>
            </div>

            {loading ? (
                <div className="loading-text">Loading releases from GitHub…</div>
            ) : filteredReleases.length === 0 ? (
                <div className="empty-state">
                    <div className="empty-state-icon">📦</div>
                    <p>{search ? 'No matching releases found' : 'No releases yet'}</p>
                </div>
            ) : (
                filteredReleases.map((r, i) => (
                    <div
                        className="release-item"
                        key={r.id}
                        style={{ animationDelay: `${0.05 * Math.min(i, 10)}s` }}
                    >
                        <div className="release-item-header">
                            <div>
                                <div className="release-item-version">
                                    {r.name || r.tag_name}
                                    {' '}
                                    <span className={`badge ${getBadgeClass(r)}`} style={{ marginLeft: 8 }}>
                                        {getBadgeLabel(r)}
                                    </span>
                                    {r.targetPrefix && (
                                        <span className="badge info" style={{ marginLeft: 4 }}>
                                            {r.targetPrefix}
                                        </span>
                                    )}
                                </div>
                                <div className="release-item-date">{formatDate(r.published_at)}</div>
                            </div>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                                {r.author && (
                                    <div className="author">
                                        {r.author.avatar_url && (
                                            // eslint-disable-next-line @next/next/no-img-element
                                            <img src={r.author.avatar_url} alt={r.author.login} className="author-avatar" />
                                        )}
                                        <span className="author-name">{r.author.login}</span>
                                    </div>
                                )}
                                <a href={r.html_url} target="_blank" rel="noopener noreferrer" style={{ fontSize: 13 }}>
                                    View Details →
                                </a>
                            </div>
                        </div>
                        {r.body && (
                            <div className="release-item-body release-markdown">
                                <ReactMarkdown>
                                    {r.body}
                                </ReactMarkdown>
                            </div>
                        )}
                        {r.assets && r.assets.length > 0 && renderAssetsGroups(r.assets)}
                    </div>
                ))
            )}

            <footer className="footer">
                <span>© 2026 Longhorn Racing Electric. Built for the track.</span>
                <div className="footer-links">
                    <a href="https://github.com/LonghornRacingElectric/lhre-2026" target="_blank" rel="noopener noreferrer">GitHub</a>
                    <a href="https://github.com/LonghornRacingElectric/lhre-2026/releases" target="_blank" rel="noopener noreferrer">All Releases</a>
                </div>
            </footer>
        </>
    );
}
