import React, { useEffect, useState } from 'react';
import { DRIVERS, useSettings } from '../context/SettingsContext';
import TopTray from '../components/TopTray';
import './ScreenOne.css';
import './Settings.css';

interface SettingDef {
    label: string;
    options: readonly string[];
    value: string;
    onChange: (next: string) => void;
}

const Settings: React.FC = () => {
    const { settings, setActiveDriver } = useSettings();
    const [focusIndex, setFocusIndex] = useState(0);

    // Add new settings by appending to this array.
    const settingsList: SettingDef[] = [
        {
            label: 'Active Driver',
            options: DRIVERS,
            value: settings.activeDriver,
            onChange: (next) => setActiveDriver(next as typeof DRIVERS[number]),
        },
    ];

    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            if (e.key === 'ArrowDown') {
                e.preventDefault();
                setFocusIndex(i => (i + 1) % settingsList.length);
            } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                setFocusIndex(i => (i - 1 + settingsList.length) % settingsList.length);
            } else if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
                e.preventDefault();
                const setting = settingsList[focusIndex];
                const idx = setting.options.indexOf(setting.value);
                const delta = e.key === 'ArrowRight' ? 1 : -1;
                const nextIdx = (idx + delta + setting.options.length) % setting.options.length;
                setting.onChange(setting.options[nextIdx]);
            }
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, [focusIndex, settingsList]);

    return (
        <div className="modern-dash-container settings-screen">
            <TopTray screenLabel="Settings" />

            <div className="settings-grid">
                {settingsList.map((s, i) => {
                    const focused = i === focusIndex;
                    return (
                        <div
                            key={s.label}
                            className={`dash-card setting-card${focused ? ' focused' : ''}`}
                            onClick={() => setFocusIndex(i)}
                        >
                            <div className="label-small">{s.label}</div>
                            <div className="setting-card-control">
                                <span className="setting-arrow">‹</span>
                                <div className="value-display setting-card-value">{s.value}</div>
                                <span className="setting-arrow">›</span>
                            </div>
                        </div>
                    );
                })}
            </div>

            <div className="label-small settings-footer">
                ← → change &nbsp;·&nbsp; ↑ ↓ select &nbsp;·&nbsp; Enter next screen
            </div>
        </div>
    );
};

export default Settings;
