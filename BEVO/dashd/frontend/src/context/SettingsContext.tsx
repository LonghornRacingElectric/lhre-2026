import React, { createContext, useContext, useEffect, useState } from 'react';

// Hardcoded driver roster. Edit here when the lineup changes.
export const DRIVERS = [
    'Oliver Belforti',
    'Luke Ballengee',
    'Andrew Cloran',
    'Shreyas Vatts',
] as const;

export type Driver = typeof DRIVERS[number];

export const THEMES = ['dark', 'light'] as const;
export type Theme = typeof THEMES[number];

interface Settings {
    activeDriver: Driver;
    theme: Theme;
}

const DEFAULT_SETTINGS: Settings = {
    activeDriver: DRIVERS[0],
    theme: 'light',
};

const STORAGE_KEY = 'dashd.settings';

function loadSettings(): Settings {
    try {
        const raw = localStorage.getItem(STORAGE_KEY);
        if (!raw) return DEFAULT_SETTINGS;
        const parsed = JSON.parse(raw);
        const driver = DRIVERS.includes(parsed.activeDriver)
            ? parsed.activeDriver
            : DEFAULT_SETTINGS.activeDriver;
        const theme: Theme = THEMES.includes(parsed.theme)
            ? parsed.theme
            : DEFAULT_SETTINGS.theme;
        return { activeDriver: driver, theme };
    } catch {
        return DEFAULT_SETTINGS;
    }
}

interface SettingsContextValue {
    settings: Settings;
    setActiveDriver: (driver: Driver) => void;
    setTheme: (theme: Theme) => void;
}

const SettingsContext = createContext<SettingsContextValue>({
    settings: DEFAULT_SETTINGS,
    setActiveDriver: () => {},
    setTheme: () => {},
});

export const useSettings = () => useContext(SettingsContext);

export const SettingsProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const [settings, setSettings] = useState<Settings>(loadSettings);

    useEffect(() => {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
    }, [settings]);

    // Apply theme to body so CSS variables in index.css switch.
    useEffect(() => {
        document.body.classList.toggle('theme-light', settings.theme === 'light');
    }, [settings.theme]);

    const setActiveDriver = (driver: Driver) => {
        setSettings(prev => ({ ...prev, activeDriver: driver }));
    };

    const setTheme = (theme: Theme) => {
        setSettings(prev => ({ ...prev, theme }));
    };

    return (
        <SettingsContext.Provider value={{ settings, setActiveDriver, setTheme }}>
            {children}
        </SettingsContext.Provider>
    );
};
