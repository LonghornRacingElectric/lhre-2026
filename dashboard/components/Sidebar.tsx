'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';

export default function Sidebar() {
    const pathname = usePathname();

    const navItems = [
        { href: '/', label: 'Mission Control', icon: 'dashboard' },
        { href: '/releases', label: 'Target Matrix', icon: 'grid_view' },
    ];

    const targetItems = [
        { href: '/targets/VCU', label: 'VCU', icon: 'memory' },
        { href: '/targets/HVC', label: 'HVC', icon: 'battery_charging_full' },
        { href: '/targets/CSM', label: 'CSM', icon: 'sensors' },
        { href: '/targets/DUI', label: 'DUI', icon: 'display_settings' },
        { href: '/targets/LVBMS', label: 'LVBMS', icon: 'battery_std' },
        { href: '/targets/TSM', label: 'TSM', icon: 'device_thermostat' },
        { href: '/targets/USM', label: 'USM', icon: 'precision_manufacturing' },
        { href: '/targets/PDU', label: 'PDU', icon: 'electrical_services' },
        { href: '/targets/BEVO', label: 'BEVO', icon: 'cell_tower' },
    ];

    return (
        <aside className="sidebar">
            <div style={{ display: 'flex', flexDirection: 'column', gap: 32 }}>
                {/* Brand */}
                <Link href="/" className="sidebar-brand" style={{ textDecoration: 'none' }}>
                    <div className="sidebar-brand-icon chamfer-sm">
                        <span className="material-symbols-outlined">electric_bolt</span>
                    </div>
                    <div className="sidebar-brand-text">
                        <span className="sidebar-brand-title">LHRe</span>
                        <span className="sidebar-brand-subtitle">FIRMWARE_CTRL</span>
                    </div>
                </Link>

                {/* Nav */}
                <nav className="sidebar-nav">
                    {navItems.map(item => (
                        <Link
                            key={item.href}
                            href={item.href}
                            className={`sidebar-link ${pathname === item.href ? 'active' : ''}`}
                        >
                            <span className="material-symbols-outlined">{item.icon}</span>
                            <span>{item.label}</span>
                        </Link>
                    ))}

                    <div style={{
                        fontSize: 10, textTransform: 'uppercase', letterSpacing: '0.15em',
                        color: 'var(--text-muted)', marginTop: 24, marginBottom: 8, paddingLeft: 12,
                        fontFamily: 'var(--font-mono)',
                    }}>
                        Targets
                    </div>
                    {targetItems.map(item => (
                        <Link
                            key={item.href}
                            href={item.href}
                            className={`sidebar-link ${pathname === item.href ? 'active' : ''}`}
                        >
                            <span className="material-symbols-outlined">{item.icon}</span>
                            <span>{item.label}</span>
                        </Link>
                    ))}

                    <div style={{
                        fontSize: 10, textTransform: 'uppercase', letterSpacing: '0.15em',
                        color: 'var(--text-muted)', marginTop: 24, marginBottom: 8, paddingLeft: 12,
                        fontFamily: 'var(--font-mono)',
                    }}>
                        External
                    </div>
                    <a
                        href="https://github.com/LonghornRacingElectric/lhre-2026/actions"
                        target="_blank"
                        rel="noopener noreferrer"
                        className="sidebar-link"
                    >
                        <span className="material-symbols-outlined">terminal</span>
                        <span>CI / Actions</span>
                    </a>
                </nav>
            </div>

            {/* Bottom */}
            <div className="sidebar-bottom">
                <div className="sidebar-status">
                    <div className="sidebar-status-header">
                        <span className="sidebar-status-label">System Status</span>
                        <span className="sidebar-status-dot" />
                    </div>
                    <div className="sidebar-status-text">
                        API: <span>CONNECTED</span>
                    </div>
                </div>
                <a
                    href="https://github.com/LonghornRacingElectric/lhre-2026"
                    target="_blank"
                    rel="noopener noreferrer"
                    className="sidebar-settings"
                >
                    <span className="material-symbols-outlined">open_in_new</span>
                    <span>GitHub Repo</span>
                </a>
            </div>
        </aside>
    );
}
