import React, { createContext, useContext, useEffect, useState } from 'react';

// Hardcoded driver roster. Edit here when the lineup changes.
export const DRIVERS = [
    'Oliver Belforti',
    'Luke Ballengee',
    'Andrew Cloran',
    'Shreyas Vatts',
] as const;

export type Driver = typeof DRIVERS[number];

interface Settings {
    activeDriver: Driver;
}

const DEFAULT_SETTINGS: Settings = {
    activeDriver: DRIVERS[0],
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
        return { activeDriver: driver };
    } catch {
        return DEFAULT_SETTINGS;
    }
}

interface SettingsContextValue {
    settings: Settings;
    setActiveDriver: (driver: Driver) => void;
}

const SettingsContext = createContext<SettingsContextValue>({
    settings: DEFAULT_SETTINGS,
    setActiveDriver: () => {},
});

export const useSettings = () => useContext(SettingsContext);

export const SettingsProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const [settings, setSettings] = useState<Settings>(loadSettings);

    useEffect(() => {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
    }, [settings]);

    const setActiveDriver = (driver: Driver) => {
        setSettings(prev => ({ ...prev, activeDriver: driver }));
    };

    return (
        <SettingsContext.Provider value={{ settings, setActiveDriver }}>
            {children}
        </SettingsContext.Provider>
    );
};
