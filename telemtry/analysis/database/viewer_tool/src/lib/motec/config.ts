// Port of config.py — settings driven by env, defaulting to safe "sample" mode.
import path from "path";

export type CarId = "orion" | "angelique";

export type Settings = {
  telemetrySource: string;
  orionDbHost: string;
  orionDbPort: number;
  orionDbName: string;
  orionDbUser: string;
  orionDbPassword: string;
  orionDbSslmode: string;
  orionDbConnectTimeout: number;
  exportDir: string;
  trackDir: string;
  channelChartDir: string;
  displayTimezone: string;
  maxPreviewPoints: number;
  maxPreviewSeconds: number;
  maxAutoSplitSeconds: number;
  maxExportSeconds: number;
  usePostgres: boolean;
};

// Repo-local data dirs live under the viewer_tool working directory.
const ROOT = process.cwd();

function envNum(name: string, fallback: number): number {
  const v = Number(process.env[name]);
  return Number.isFinite(v) && v > 0 ? v : fallback;
}

function baseSettings(): Settings {
  const telemetrySource = (process.env.TELEMETRY_SOURCE || "sample").trim().toLowerCase();
  return {
    telemetrySource,
    orionDbHost: process.env.ORION_DB_HOST || "192.168.1.109",
    orionDbPort: envNum("ORION_DB_PORT", 5432),
    orionDbName: process.env.ORION_DB_NAME || "orion",
    orionDbUser: process.env.ORION_DB_USER || "analysis",
    orionDbPassword: process.env.ORION_DB_PASSWORD || "",
    orionDbSslmode: process.env.ORION_DB_SSLMODE || "disable",
    orionDbConnectTimeout: envNum("ORION_DB_CONNECT_TIMEOUT", 5),
    exportDir: process.env.EXPORT_DIR || path.join(ROOT, "exports"),
    trackDir: process.env.TRACK_DIR || path.join(ROOT, "tracks"),
    channelChartDir: process.env.CHANNEL_CHART_DIR || path.join(ROOT, "channel_charts"),
    displayTimezone: process.env.DISPLAY_TIMEZONE || "America/Chicago",
    maxPreviewPoints: envNum("MAX_PREVIEW_POINTS", 5000),
    maxPreviewSeconds: envNum("MAX_PREVIEW_SECONDS", 60 * 60 * 2),
    maxAutoSplitSeconds: envNum("MAX_AUTO_SPLIT_SECONDS", 60 * 30),
    maxExportSeconds: envNum("MAX_EXPORT_SECONDS", 60 * 30),
    usePostgres: telemetrySource === "postgres",
  };
}

export function getSettings(source?: string | null): Settings {
  const settings = baseSettings();
  const normalized = (source || "orion").trim().toLowerCase();
  if (normalized !== "orion" && normalized !== "angelique") {
    throw new Error(`Unknown telemetry source: ${source}`);
  }
  if (normalized === "angelique") {
    return {
      ...settings,
      orionDbHost: process.env.ANGELIQUE_DB_HOST || settings.orionDbHost,
      orionDbPort: envNum("ANGELIQUE_DB_PORT", settings.orionDbPort),
      orionDbName: process.env.ANGELIQUE_DB_NAME || "angelique",
      orionDbUser: process.env.ANGELIQUE_DB_USER || settings.orionDbUser,
      orionDbPassword: process.env.ANGELIQUE_DB_PASSWORD || settings.orionDbPassword,
      orionDbSslmode: process.env.ANGELIQUE_DB_SSLMODE || settings.orionDbSslmode,
    };
  }
  return settings;
}
