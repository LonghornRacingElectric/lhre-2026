export type LiveCar = "orion" | "angelique";

export const SUPPORTED_LIVE_CARS: LiveCar[] = ["orion", "angelique"];
const DISABLED_FLAG_VALUES = new Set(["0", "false", "no", "off"]);

export function normalizeLiveCar(value: string | null | undefined): LiveCar | null {
  if (!value) return null;
  const normalized = value.trim().toLowerCase();
  if (normalized === "orion") return "orion";
  if (normalized === "angelique") return "angelique";
  return null;
}

export function liveCarLabel(car: LiveCar): string {
  if (car === "orion") return "Orion";
  return "Angelique";
}

export function isMultiCarViewerEnabled(): boolean {
  const raw = process.env.NEXT_PUBLIC_VIEWER_MULTI_CAR;
  if (typeof raw !== "string") return true;
  return !DISABLED_FLAG_VALUES.has(raw.trim().toLowerCase());
}
