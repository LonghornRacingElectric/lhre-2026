'use client';

import { useState } from 'react';

interface FlashModalProps {
    targetId: string;
    targetName: string;
    version: string;
    sha: string;
    onClose: () => void;
}

export default function FlashModal({ targetId, targetName, version, sha, onClose }: FlashModalProps) {
    const [check1, setCheck1] = useState(false);
    const [check2, setCheck2] = useState(false);
    const allChecked = check1 && check2;

    return (
        <div className="modal-overlay" onClick={onClose}>
            <div className="modal-container" onClick={e => e.stopPropagation()}>
                <div className="modal-corner tl" />
                <div className="modal-corner tr" />
                <div className="modal-corner bl" />
                <div className="modal-corner br" />

                {/* Header */}
                <div className="modal-header">
                    <div className="modal-title">
                        <span className="material-symbols-outlined" style={{ color: 'var(--primary)', animation: 'pulse 2s infinite' }}>terminal</span>
                        // FLASH PROTOCOL INITIATED
                    </div>
                    <button className="modal-close" onClick={onClose}>
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                {/* Content */}
                <div className="modal-body">
                    {/* Target Info Grid */}
                    <div className="modal-info-grid">
                        <div className="modal-info-card">
                            <span className="modal-info-label">TARGET ID</span>
                            <div className="modal-info-value">
                                <span className="material-symbols-outlined">memory</span>
                                {targetName}
                            </div>
                        </div>
                        <div className="modal-info-card">
                            <span className="modal-info-label">PAYLOAD VERSION</span>
                            <div className="modal-info-value">
                                <span className="material-symbols-outlined">deployed_code</span>
                                {version} <span style={{ fontSize: 13, fontWeight: 400, fontFamily: 'var(--font-mono)', color: 'var(--text-muted)' }}>(sha: {sha})</span>
                            </div>
                        </div>
                    </div>

                    {/* Safety Interlocks */}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                        <div className="interlock-header">
                            <span className="interlock-title">Safety Interlocks</span>
                            <span className="interlock-required">
                                <span className="material-symbols-outlined">warning</span>
                                REQUIRED
                            </span>
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                            <label className="interlock-item">
                                <input type="checkbox" checked={check1} onChange={() => setCheck1(!check1)} />
                                <div className="interlock-item-text">
                                    <span className="interlock-item-title">CONFIRM HV IS ISOLATED</span>
                                    <span className="interlock-item-desc">High Voltage lockout tagout procedure verified.</span>
                                </div>
                            </label>
                            <label className="interlock-item">
                                <input type="checkbox" checked={check2} onChange={() => setCheck2(!check2)} />
                                <div className="interlock-item-text">
                                    <span className="interlock-item-title">CONFIRM TRACTIVE SYSTEM IS OFF</span>
                                    <span className="interlock-item-desc">Master switch in OFF position. Key removed.</span>
                                </div>
                            </label>
                        </div>
                    </div>

                    {/* Terminal Log */}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-end' }}>
                            <span style={{ fontFamily: 'var(--font-mono)', fontSize: 11, color: 'var(--text-muted)', textTransform: 'uppercase' }}>System Log Output</span>
                            <span style={{ fontFamily: 'var(--font-mono)', fontSize: 11, color: 'var(--success)', animation: 'pulse 2s infinite' }}>● LIVE CONNECTION</span>
                        </div>
                        <div className="terminal-box">
                            <div className="scanline-overlay" />
                            <div className="terminal-scroll">
                                <div className="terminal-line"><span className="terminal-time">--:--:--.---</span><span>&gt; INITIATING PROTOCOL HANDSHAKE...</span></div>
                                <div className="terminal-line"><span className="terminal-time">--:--:--.045</span><span>&gt; TARGET: {targetId} DETECTED</span></div>
                                <div className="terminal-line"><span className="terminal-time">--:--:--.112</span><span>&gt; TARGET VOLTAGE: 3.3V [OK]</span></div>
                                <div className="terminal-line"><span className="terminal-time">--:--:--.230</span><span>&gt; VERIFYING INTEGRITY...</span></div>
                                <div className="terminal-line success"><span className="terminal-time">--:--:--.405</span><span>&gt; CHECKSUM MATCH: 0x{sha.toUpperCase()}</span></div>
                                <div className="terminal-line"><span className="terminal-time">--:--:--.500</span><span>&gt; AWAITING SAFETY INTERLOCK CONFIRMATION...</span></div>
                                <div className="terminal-cursor">
                                    <span style={{ color: 'var(--primary)' }}>&gt;</span>
                                    <span className="terminal-cursor-block" />
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                {/* Footer Actions */}
                <div className="modal-footer">
                    <button className="btn-abort" onClick={onClose}>
                        <span className="material-symbols-outlined">close</span>
                        Abort Sequence
                    </button>
                    <a
                        href={`https://github.com/LonghornRacingElectric/lhre-2026/actions`}
                        target="_blank"
                        rel="noopener noreferrer"
                        className={`btn-execute ${allChecked ? 'enabled' : ''}`}
                        style={{ textDecoration: 'none' }}
                        onClick={e => { if (!allChecked) e.preventDefault(); }}
                    >
                        <span className="material-symbols-outlined" style={{ fontSize: 18 }}>bolt</span>
                        Execute Flash
                        {!allChecked && <span className="btn-execute-lock">LOCKED</span>}
                    </a>
                </div>
            </div>
        </div>
    );
}
