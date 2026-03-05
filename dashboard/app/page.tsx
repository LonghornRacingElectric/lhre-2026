'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import {
  fetchWorkflowRuns, fetchCommits,
  FIRMWARE_TARGETS, type WorkflowRun, type Commit,
} from '@/lib/github';

export default function MissionControlPage() {
  const [runs, setRuns] = useState<{ total_count: number; workflow_runs: WorkflowRun[] }>({ total_count: 0, workflow_runs: [] });
  const [commits, setCommits] = useState<Commit[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    Promise.all([fetchWorkflowRuns(), fetchCommits(7)])
      .then(([w, c]) => {
        setRuns(w);
        setCommits(c);
        setLoading(false);
      })
      .catch(() => setLoading(false));
  }, []);

  const recentCommits = commits.slice(0, 5);
  const recentRuns = runs.workflow_runs.slice(0, 10);
  const failingCount = recentRuns.filter(r => r.conclusion === 'failure').length;
  const openPRs = runs.total_count; // approximate - using total workflow runs as a proxy

  // Build status from workflow runs
  const targetBuildStatus: Record<string, string> = {};
  for (const run of runs.workflow_runs) {
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

  function getStatusClass(conclusion: string | null): string {
    if (!conclusion) return '';
    if (conclusion === 'success') return 'success';
    if (conclusion === 'failure') return 'error';
    return '';
  }

  function getStatusIcon(run: WorkflowRun): { icon: string; cls: string } {
    if (!run.conclusion || run.status === 'in_progress' || run.status === 'queued') {
      return { icon: 'progress_activity', cls: 'running' };
    }
    if (run.conclusion === 'success') return { icon: 'check_circle', cls: 'ok' };
    if (run.conclusion === 'failure') return { icon: 'cancel', cls: 'fail' };
    return { icon: 'help', cls: '' };
  }

  return (
    <>
      {/* Header Bar */}
      <div className="page-header-bar">
        <div className="page-header-left">
          <h1 className="page-header-title">LHRe // Firmware Control</h1>
        </div>
        <div className="page-header-right">
          <div className="connection-pill">
            <div className="connection-dot-wrap">
              <span className="connection-dot-ping" />
              <span className="connection-dot" />
            </div>
            <span className="connection-text">API: CONNECTED</span>
          </div>
          <div className="user-area">
            <div className="user-avatar">
              <span className="material-symbols-outlined">person</span>
            </div>
          </div>
        </div>
      </div>

      {/* Main Grid */}
      <div style={{
        flex: 1, overflow: 'hidden', padding: 24,
        display: 'grid', gridTemplateColumns: '3fr 5fr 4fr', gap: 24,
      }}>
        {/* LEFT COLUMN: Repo Health */}
        <section style={{ display: 'flex', flexDirection: 'column', gap: 24, height: '100%', overflowY: 'auto', paddingRight: 4 }}>
          <div className="section-bar">
            <h2>REPO HEALTH</h2>
            <span className="material-symbols-outlined">monitor_heart</span>
          </div>

          {/* KPIs */}
          <div className="kpi-grid">
            <div className="kpi-card">
              <div className="kpi-icon">
                <span className="material-symbols-outlined">deployed_code</span>
              </div>
              <span className="kpi-label">Total Builds</span>
              <span className="kpi-value">{loading ? '…' : String(runs.total_count).padStart(2, '0')}</span>
            </div>
            <div className="kpi-card">
              <div className="kpi-icon" style={{ color: 'var(--error)' }}>
                <span className="material-symbols-outlined">gpp_maybe</span>
              </div>
              <span className="kpi-label">Failing</span>
              <span className="kpi-value error">{loading ? '…' : String(failingCount).padStart(2, '0')}</span>
            </div>
          </div>

          {/* Commit Ticker */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: 12, flex: 1 }}>
            <h3 style={{
              fontSize: 11, fontFamily: 'var(--font-mono)', textTransform: 'uppercase',
              letterSpacing: '0.1em', color: 'var(--text-muted)',
              display: 'flex', alignItems: 'center', gap: 8,
            }}>
              <span style={{ width: 4, height: 4, background: 'var(--primary)', borderRadius: '50%' }} />
              Recent Commits
            </h3>
            <div className="commit-list">
              {loading ? (
                <div className="loading-text">Loading…</div>
              ) : recentCommits.length === 0 ? (
                <div className="empty-state"><p>No recent commits</p></div>
              ) : (
                recentCommits.map((c, i) => (
                  <a key={c.sha} href={c.html_url} target="_blank" rel="noopener noreferrer" className="commit-item" style={{ textDecoration: 'none', color: 'inherit' }}>
                    <div className="commit-item-header">
                      <span className={`commit-sha ${i > 0 ? 'muted' : ''}`}>{c.sha.substring(0, 6)}</span>
                      <span className="commit-time">{timeAgo(c.commit.author.date)}</span>
                    </div>
                    <p className="commit-msg">{c.commit.message.split('\n')[0]}</p>
                    <div className="commit-author">
                      <div className="commit-author-avatar">
                        {c.author?.avatar_url && (
                          // eslint-disable-next-line @next/next/no-img-element
                          <img src={c.author.avatar_url} alt="" />
                        )}
                      </div>
                      <span className="commit-author-name">@{c.author?.login || c.commit.author.name}</span>
                    </div>
                  </a>
                ))
              )}
            </div>
          </div>

          {/* Quick Action */}
          <div style={{ marginTop: 'auto', paddingTop: 16, borderTop: '1px solid var(--stroke)' }}>
            <a
              href="https://github.com/LonghornRacingElectric/lhre-2026/commits/main"
              target="_blank"
              rel="noopener noreferrer"
              style={{
                width: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 8,
                background: 'var(--bg-surface)', border: '1px solid var(--stroke)', color: 'var(--text-muted)',
                padding: '8px 16px', fontSize: 13, fontWeight: 500, textTransform: 'uppercase',
                letterSpacing: '0.05em', transition: 'all 0.2s', textDecoration: 'none',
              }}
            >
              <span className="material-symbols-outlined" style={{ fontSize: 16 }}>history</span>
              View All History
            </a>
          </div>
        </section>

        {/* CENTER COLUMN: Live Action Feed */}
        <section style={{
          display: 'flex', flexDirection: 'column', gap: 24, height: '100%',
          borderLeft: '1px solid rgba(48, 54, 61, 0.3)', borderRight: '1px solid rgba(48, 54, 61, 0.3)',
          paddingLeft: 24, paddingRight: 24,
        }}>
          <div className="section-bar">
            <h2>LIVE ACTION FEED</h2>
            <div className="live-indicator">
              <span className="live-dot" />
              <span className="live-text">{loading ? 'LOADING' : 'LIVE'}</span>
            </div>
          </div>

          <div className="feed-list" style={{ flex: 1 }}>
            {loading ? (
              <div className="loading-text">Loading workflow runs…</div>
            ) : recentRuns.length === 0 ? (
              <div className="empty-state"><p>No recent workflow runs</p></div>
            ) : (
              recentRuns.map(run => {
                const status = getStatusIcon(run);
                const isRunning = !run.conclusion || run.status === 'in_progress';
                return (
                  <a
                    key={run.id}
                    href={run.html_url}
                    target="_blank"
                    rel="noopener noreferrer"
                    className={`feed-item ${getStatusClass(run.conclusion)}`}
                    style={{ textDecoration: 'none', color: 'inherit' }}
                  >
                    <div className="feed-item-header">
                      <div style={{ display: 'flex', flexDirection: 'column' }}>
                        <h4 className="feed-item-title">
                          {run.name}
                          <span className={`feed-item-tag ${isRunning ? 'primary' : 'muted'}`}>
                            {run.head_branch}
                          </span>
                        </h4>
                        <span className="feed-item-sub">
                          #{run.run_number} • {timeAgo(run.created_at)}
                        </span>
                      </div>
                      <div className={`feed-item-status ${status.cls}`}>
                        <span className="material-symbols-outlined">{status.icon}</span>
                      </div>
                    </div>
                    {isRunning && (
                      <>
                        <div className="progress-bar">
                          <div className="progress-fill striped" style={{ width: '50%' }} />
                        </div>
                        <div className="progress-meta">
                          <span>Running…</span>
                          <span>—</span>
                        </div>
                      </>
                    )}
                  </a>
                );
              })
            )}
          </div>
        </section>

        {/* RIGHT COLUMN: Target Health */}
        <section style={{ display: 'flex', flexDirection: 'column', gap: 24, height: '100%' }}>
          <div className="section-bar">
            <h2>TARGET HEALTH</h2>
            <span className="material-symbols-outlined">hub</span>
          </div>

          {/* Car Map */}
          <div className="car-map">
            <div className="car-map-grid" />
            {/* Vehicle SVG */}
            <svg width="180" height="320" viewBox="0 0 240 420" fill="none" xmlns="http://www.w3.org/2000/svg" style={{ color: 'var(--stroke)', filter: 'drop-shadow(0 10px 30px rgba(0,0,0,0.5))', zIndex: 0 }}>
              <path d="M120 20 L140 80 H100 L120 20 Z" stroke="currentColor" strokeWidth="2" />
              <path d="M60 40 H180 L190 60 H50 L60 40 Z" stroke="currentColor" strokeWidth="2" />
              <path d="M100 80 H140 L160 200 L170 320 H70 L80 200 L100 80 Z" stroke="currentColor" strokeWidth="2" />
              <path d="M140 160 L180 180 V280 L165 300" stroke="currentColor" strokeWidth="2" />
              <path d="M100 160 L60 180 V280 L75 300" stroke="currentColor" strokeWidth="2" />
              <path d="M60 380 H180 V400 H60 V380 Z" stroke="currentColor" strokeWidth="2" />
              <rect x="20" y="80" width="30" height="50" rx="4" stroke="currentColor" strokeWidth="2" />
              <rect x="190" y="80" width="30" height="50" rx="4" stroke="currentColor" strokeWidth="2" />
              <rect x="20" y="300" width="40" height="60" rx="4" stroke="currentColor" strokeWidth="2" />
              <rect x="180" y="300" width="40" height="60" rx="4" stroke="currentColor" strokeWidth="2" />
            </svg>
            {/* Nodes */}
            {[
              { id: 'VCU', top: '18%', left: '50%' },
              { id: 'CSM', top: '25%', right: '18%' },
              { id: 'USM', top: '25%', left: '18%' },
              { id: 'DUI', top: '35%', left: '50%' },
              { id: 'HVC', top: '50%', left: '50%' },
              { id: 'LVBMS', top: '60%', right: '22%' },
              { id: 'TSM', top: '60%', left: '22%' },
              { id: 'PDU', top: '75%', left: '50%' },
              { id: 'BEVO', top: '85%', left: '50%' },
            ].map(node => {
              const status = targetBuildStatus[node.id];
              const cls = status === 'success' ? 'success' : status === 'failure' ? 'error' : 'warning';
              const label = status === 'success' ? 'PASSING' : status === 'failure' ? 'FAILING' : 'UNKNOWN';
              const pos: React.CSSProperties = { top: node.top, transform: 'translate(-50%, -50%)' };
              if (node.left) pos.left = node.left;
              if (node.right) pos.right = node.right;
              if (node.right) pos.transform = 'translate(50%, -50%)';
              return (
                <Link key={node.id} href={`/targets/${node.id}`} className="car-map-node" style={pos}>
                  <div className="car-map-dot-wrap">
                    {cls === 'success' && <span className="car-map-dot-ping" style={{ background: 'var(--success)', animation: 'ping 1s cubic-bezier(0, 0, 0.2, 1) infinite' }} />}
                    <span className={`car-map-dot ${cls}`} />
                  </div>
                  <div className="car-map-tooltip" style={{ borderColor: `var(--${cls === 'error' ? 'error' : cls === 'warning' ? 'warning' : 'success'})`, top: -32, left: '50%', transform: 'translateX(-50%)' }}>
                    <span className="car-map-tooltip-name">{node.id}</span>
                    <span className="car-map-tooltip-status" style={{ color: `var(--${cls === 'error' ? 'error' : cls === 'warning' ? 'warning' : 'success'})` }}>{label}</span>
                  </div>
                </Link>
              );
            })}
          </div>

          {/* Target List */}
          <div className="target-list" style={{ overflowY: 'auto', flex: 1 }}>
            {FIRMWARE_TARGETS.map(target => {
              const status = targetBuildStatus[target.id];
              const badgeCls = status === 'success' ? 'stable' : status === 'failure' ? 'error' : 'warning';
              const badgeLabel = status === 'success' ? 'PASSING' : status === 'failure' ? 'FAILING' : status === 'in_progress' ? 'RUNNING' : 'UNKNOWN';
              return (
                <Link key={target.id} href={`/targets/${target.id}`} className="target-list-item">
                  <div className="target-list-item-info">
                    <span className="target-list-item-name">{target.fullName} ({target.name})</span>
                    <span className="target-list-item-chip">Target: {target.chip}</span>
                  </div>
                  <span className={`status-badge ${badgeCls}`}>{badgeLabel}</span>
                </Link>
              );
            })}
          </div>
        </section>
      </div>

      {/* Footer */}
      <footer className="app-footer">
        <div>© 2026 Longhorn Racing Electric</div>
        <div className="app-footer-right">
          <a href="https://github.com/LonghornRacingElectric/lhre-2026" target="_blank" rel="noopener noreferrer" style={{ color: 'var(--text-muted)' }}>GitHub</a>
        </div>
      </footer>
    </>
  );
}
