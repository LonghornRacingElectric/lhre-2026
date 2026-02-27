'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import {
    fetchReleases, getGlobalReleases, getLatestRelease,
    type Release,
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
                                <div className="release-body">
                                    {latestStable.body.split('\n').slice(0, 5).map((line, i) => (
                                        <span key={i}>{line}<br /></span>
                                    ))}
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
                            <div className="release-actions">
                                <a href={latestStable.html_url} target="_blank" rel="noopener noreferrer" className="btn btn-primary">
                                    ⬇ Download Artifacts
                                </a>
                                <a href={latestStable.html_url} target="_blank" rel="noopener noreferrer" className="btn btn-secondary">
                                    📝 Release Notes
                                </a>
                            </div>
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
                            <div className="release-item-body">
                                {r.body.split('\n').slice(0, 6).map((line, j) => (
                                    <span key={j}>{line}<br /></span>
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
