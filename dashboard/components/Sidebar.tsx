'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';

export default function Sidebar() {
    const pathname = usePathname();

    const navItems = [
        { href: '/', label: 'Dashboard', icon: '📊' },
        { href: '/releases', label: 'Releases', icon: '🏷️' },
    ];

    const targetItems = [
        { href: '/targets/VCU', label: 'VCU', icon: '🔧' },
        { href: '/targets/HVC', label: 'HVC', icon: '⚡' },
        { href: '/targets/CSM', label: 'CSM', icon: '❄️' },
        { href: '/targets/DUI', label: 'DUI', icon: '🖥️' },
        { href: '/targets/LVBMS', label: 'LVBMS', icon: '🔋' },
        { href: '/targets/TSM', label: 'TSM', icon: '⚠️' },
        { href: '/targets/USM', label: 'USM', icon: '🛡️' },
        { href: '/targets/PDU', label: 'PDU', icon: '🔌' },
        { href: '/targets/BEVO', label: 'BEVO', icon: '🤖' },
    ];

    return (
        <aside className="sidebar">
            <Link href="/" className="sidebar-logo" style={{ textDecoration: 'none' }}>
                <div className="sidebar-logo-icon">L</div>
                <div className="sidebar-logo-text">
                    <span className="sidebar-logo-title">lhre-2026</span>
                    <span className="sidebar-logo-subtitle">Monorepo Dashboard</span>
                </div>
            </Link>

            <nav className="sidebar-nav">
                <div className="sidebar-section-label">Overview</div>
                {navItems.map(item => (
                    <Link
                        key={item.href}
                        href={item.href}
                        className={`sidebar-link ${pathname === item.href ? 'active' : ''}`}
                    >
                        <span className="sidebar-link-icon">{item.icon}</span>
                        {item.label}
                    </Link>
                ))}

                <div className="sidebar-section-label">Firmware Targets</div>
                {targetItems.map(item => (
                    <Link
                        key={item.href}
                        href={item.href}
                        className={`sidebar-link ${pathname === item.href ? 'active' : ''}`}
                    >
                        <span className="sidebar-link-icon">{item.icon}</span>
                        {item.label}
                    </Link>
                ))}
            </nav>
        </aside>
    );
}
