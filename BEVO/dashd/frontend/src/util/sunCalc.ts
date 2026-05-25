// Inline sunrise/sunset computation. NOAA Solar Position Algorithm
// (the simplified form documented at https://gml.noaa.gov/grad/solcalc/).
// Accurate to about ±2 minutes — plenty for a "is it day or night" gate.
//
// Austin, TX coordinates (UT Austin / Longhorn Racing). If the car ever
// moves significantly, swap these or feed them from GPS once available.
export const AUSTIN_LAT_DEG = 30.27;
export const AUSTIN_LON_WEST_DEG = 97.74; // NOAA convention: positive west

// Civil twilight buffer: keep "light" mode for a window past sunset and
// before sunrise so the theme doesn't flip at the exact horizon moment
// when there's still useful ambient light.
const CIVIL_TWILIGHT_MS = 15 * 60 * 1000;

export interface SunWindow {
    sunrise: Date | null; // null = polar day / night (won't happen in Austin)
    sunset: Date | null;
}

export function computeSunWindow(
    date: Date,
    latDeg: number = AUSTIN_LAT_DEG,
    lonWestDeg: number = AUSTIN_LON_WEST_DEG,
): SunWindow {
    // Day of year (1..366).
    const start = Date.UTC(date.getUTCFullYear(), 0, 0);
    const diff = date.getTime() - start;
    const dayOfYear = Math.floor(diff / 86400000);

    // Fractional year (radians).
    const gamma = (2 * Math.PI / 365) * (dayOfYear - 1 + (date.getUTCHours() - 12) / 24);

    // Equation of time (minutes).
    const eqtime = 229.18 * (
        0.000075
        + 0.001868 * Math.cos(gamma)
        - 0.032077 * Math.sin(gamma)
        - 0.014615 * Math.cos(2 * gamma)
        - 0.040849 * Math.sin(2 * gamma)
    );

    // Solar declination (radians).
    const decl = 0.006918
        - 0.399912 * Math.cos(gamma)
        + 0.070257 * Math.sin(gamma)
        - 0.006758 * Math.cos(2 * gamma)
        + 0.000907 * Math.sin(2 * gamma)
        - 0.002697 * Math.cos(3 * gamma)
        + 0.00148 * Math.sin(3 * gamma);

    // Hour angle at sunrise/sunset, with 90.833° zenith for refraction.
    const latRad = latDeg * Math.PI / 180;
    const zenith = 90.833 * Math.PI / 180;
    const cosHa = (Math.cos(zenith) - Math.sin(latRad) * Math.sin(decl))
                  / (Math.cos(latRad) * Math.cos(decl));
    if (cosHa > 1 || cosHa < -1) return { sunrise: null, sunset: null };

    const haDeg = Math.acos(cosHa) * 180 / Math.PI;

    // Sunrise/sunset in UTC minutes from midnight. NOAA's published
    // form is `720 - 4*(lon + ha) - eqtime` but only when `lon` is in
    // degrees EAST (positive east). With longitude-west as positive
    // (our convention here), the equivalent is `720 + 4*(lon_west ∓ ha)`
    // — verified for Austin against expected sunrise ~6:35 AM CDT and
    // sunset ~8:20 PM CDT in late May. Earlier draft used the wrong
    // sign and reported sunrise=evening / sunset=morning.
    const sunriseUtcMin = 720 + 4 * (lonWestDeg - haDeg) - eqtime;
    const sunsetUtcMin  = 720 + 4 * (lonWestDeg + haDeg) - eqtime;

    const utcMidnight = Date.UTC(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate());
    return {
        sunrise: new Date(utcMidnight + sunriseUtcMin * 60000),
        sunset:  new Date(utcMidnight + sunsetUtcMin  * 60000),
    };
}

// Returns 'light' or 'dark' for the given moment, applying the civil
// twilight buffer so the theme is "light" from sunrise - buffer to
// sunset + buffer. Falls back to 'dark' if the sun calc somehow fails.
export function effectiveAutoTheme(now: Date = new Date()): 'light' | 'dark' {
    const { sunrise, sunset } = computeSunWindow(now);
    if (!sunrise || !sunset) return 'dark';
    const t = now.getTime();
    const lightStart = sunrise.getTime() - CIVIL_TWILIGHT_MS;
    const lightEnd   = sunset.getTime()  + CIVIL_TWILIGHT_MS;
    return t >= lightStart && t < lightEnd ? 'light' : 'dark';
}
