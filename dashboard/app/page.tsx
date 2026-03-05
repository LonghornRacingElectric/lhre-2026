'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import {
  fetchReleases, fetchWorkflowRuns, fetchCommits,
  groupCommitsByDay, getLatestRelease,
  FIRMWARE_TARGETS, type Release, type WorkflowRun, type Commit,
} from '@/lib/github';

export default function DashboardPage() {
  const [releases, setReleases] = useState<Release[]>([]);
  const [runs, setRuns] = useState<{ total_count: number; workflow_runs: WorkflowRun[] }>({ total_count: 0, workflow_runs: [] });
  const [commits, setCommits] = useState<Commit[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    Promise.all([fetchReleases(), fetchWorkflowRuns(), fetchCommits(7)])
      .then(([r, w, c]) => {
        setReleases(r);
        setRuns(w);
        setCommits(c);
        setLoading(false);
      })
      .catch(() => setLoading(false));
  }, []);

  const latestGlobal = getLatestRelease(releases.filter(r => !r.targetPrefix));
  const successRuns = runs.workflow_runs.filter(r => r.conclusion === 'success').length;
  const totalRuns = runs.workflow_runs.length;
  const successRate = totalRuns > 0 ? ((successRuns / totalRuns) * 100).toFixed(1) : '—';
  const commitsByDay = groupCommitsByDay(commits, 7);
  const maxCommits = Math.max(...commitsByDay.map(d => d.count), 1);

  const recentReleases = [...releases]
    .sort((a, b) => new Date(b.published_at).getTime() - new Date(a.published_at).getTime())
    .slice(0, 4);

  // Build a map of latest tag per target
  const targetLatestTag: Record<string, string> = {};
  for (const r of releases) {
    if (r.targetPrefix) {
      const targetId = r.targetPrefix.split('/')[0];
      if (!targetLatestTag[targetId]) {
        targetLatestTag[targetId] = r.version || r.tag_name;
      }
    }
  }

  // Build status from workflow runs
  const targetBuildStatus: Record<string, string> = {};
  for (const run of runs.workflow_runs) {
    if (run.head_branch !== 'main') continue;

    const isGlobal = ["presubmit", "postsubmit", "release"].some(w => run.name?.toLowerCase().includes(w));

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

  return (
    <>
      <div className="page-header">
        <h1 className="page-title">Dashboard Overview</h1>
        <p className="page-subtitle">
          Vehicle performance and repository health monitoring for the 2026 season.
        </p>
      </div>

      {/* Stats Grid */}
      <div className="stats-grid">
        <div className="stat-card">
          <div className="stat-label">
            <span className="stat-dot blue" /> Total Builds
          </div>
          <div className="stat-value">{loading ? '…' : runs.total_count.toLocaleString()}</div>
          <div className="stat-trend neutral">All time</div>
        </div>
        <div className="stat-card">
          <div className="stat-label">
            <span className="stat-dot green" /> CI Success Rate
          </div>
          <div className="stat-value">{loading ? '…' : `${successRate}%`}</div>
          <div className="stat-trend up">Last 30 runs</div>
        </div>
        <div className="stat-card">
          <div className="stat-label">
            <span className="stat-dot orange" /> Active Controllers
          </div>
          <div className="stat-value">{FIRMWARE_TARGETS.length}</div>
          <div className="stat-trend neutral">Firmware targets</div>
        </div>
        <div className="stat-card">
          <div className="stat-label">
            <span className="stat-dot purple" /> Last Global Release
          </div>
          <div className="stat-value" style={{ fontSize: latestGlobal ? 22 : 28 }}>
            {loading ? '…' : latestGlobal ? latestGlobal.version || latestGlobal.tag_name : 'None'}
          </div>
          <div className="stat-trend neutral">
            {latestGlobal ? timeAgo(latestGlobal.published_at) : ''}
          </div>
        </div>
      </div>

      {/* Two Column: Activity + Recent Releases */}
      <div className="two-col">
        <div className="section-card" style={{ animationDelay: '0.2s' }}>
          <div className="section-header">
            <h2 className="section-title">Commit Activity</h2>
            <span className="section-action">Last 7 Days</span>
          </div>
          <div className="chart-container">
            <svg className="chart-svg" viewBox="0 0 500 180" preserveAspectRatio="xMidYMid meet">
              <defs>
                <linearGradient id="barGrad" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor="#3d9bf5" />
                  <stop offset="100%" stopColor="#137fec" />
                </linearGradient>
                <linearGradient id="areaGrad" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor="rgba(19,127,236,0.3)" />
                  <stop offset="100%" stopColor="rgba(19,127,236,0.02)" />
                </linearGradient>
              </defs>
              {commitsByDay.map((d, i) => {
                const barWidth = 40;
                const gap = (500 - barWidth * 7) / 8;
                const x = gap + i * (barWidth + gap);
                const barHeight = (d.count / maxCommits) * 120;
                const y = 145 - barHeight;
                return (
                  <g key={i}>
                    <rect
                      className="chart-bar"
                      x={x}
                      y={y}
                      width={barWidth}
                      height={barHeight}
                      rx={4}
                      fill="url(#barGrad)"
                      opacity={0.9}
                    />
                    <text className="chart-label" x={x + barWidth / 2} y={165} textAnchor="middle">
                      {d.day}
                    </text>
                    {d.count > 0 && (
                      <text className="chart-value" x={x + barWidth / 2} y={y - 6} textAnchor="middle">
                        {d.count}
                      </text>
                    )}
                  </g>
                );
              })}
            </svg>
          </div>
        </div>

        <div className="section-card" style={{ animationDelay: '0.3s' }}>
          <div className="section-header">
            <h2 className="section-title">Recent Releases</h2>
            <Link href="/releases" className="section-action" style={{ textDecoration: 'none' }}>
              View All Activity
            </Link>
          </div>
          {loading ? (
            <div className="loading-text">Loading…</div>
          ) : recentReleases.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">📦</div>
              <p>No releases yet</p>
            </div>
          ) : (
            recentReleases.map(r => (
              <div className="recent-release" key={r.id}>
                <div className={`recent-release-icon ${r.prerelease ? 'pre' : 'stable'}`} />
                <div className="recent-release-info">
                  <div className="recent-release-name">
                    {r.name || r.tag_name}
                  </div>
                  <div className="recent-release-meta">
                    {r.author?.login ? `By ${r.author.login}` : ''} · {timeAgo(r.published_at)}
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Firmware Targets */}
      <div className="section-header" style={{ marginTop: 'var(--space-2xl)' }}>
        <h2 className="section-title">Firmware Targets</h2>
      </div>
      <div className="targets-grid">
        {FIRMWARE_TARGETS.map((target, i) => {
          const status = targetBuildStatus[target.id];
          const latestVer = targetLatestTag[target.id];
          return (
            <Link
              key={target.id}
              href={`/targets/${target.id}`}
              className="target-card"
              style={{ animationDelay: `${0.05 * i}s` }}
            >
              <div className="target-card-header">
                <div className={`target-icon ${target.id.toLowerCase()}`}>
                  {target.id.substring(0, 3)}
                </div>
                {status && (
                  <span className={`badge ${status === 'success' ? 'success' : status === 'failure' ? 'failed' : 'info'}`}>
                    <span className="badge-dot" />
                    {status === 'success' ? 'Passing' : status === 'failure' ? 'Failed' : 'Running'}
                  </span>
                )}
              </div>
              <div className="target-name">{target.fullName}</div>
              <div className="target-desc">{target.description}</div>
              <div className="target-meta">
                <div className="target-meta-item">
                  <span className="target-meta-label">Latest Tag</span>
                  <span className="target-meta-value">{latestVer || '—'}</span>
                </div>
                <div className="target-meta-item">
                  <span className="target-meta-label">Chip</span>
                  <span className="target-meta-value">{target.chip}</span>
                </div>
              </div>
              <div className="view-details">
                View Details →
              </div>
            </Link>
          );
        })}
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
