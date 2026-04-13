"use client";

import React, {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
} from "react";

import {
  LiveCar,
  isMultiCarViewerEnabled,
  liveCarLabel,
  normalizeLiveCar,
  SUPPORTED_LIVE_CARS,
} from "@/lib/car";

const STORAGE_KEY = "liveViewerSelectedCar";
const DEFAULT_CAR =
  normalizeLiveCar(process.env.NEXT_PUBLIC_DEFAULT_LIVE_CAR) ?? "orion";
const MULTI_CAR_ENABLED = isMultiCarViewerEnabled();

type KafkaEventLike = {
  headers?: Record<string, string | undefined>;
  payload?: string;
};

type CarSelectionContextValue = {
  selectedCar: LiveCar;
  selectedCarLabel: string;
  multiCarEnabled: boolean;
  setSelectedCar: (car: LiveCar) => void;
  ssePath: string;
  matchesSelectedCar: (evt: KafkaEventLike) => boolean;
};

const CarSelectionContext = createContext<CarSelectionContextValue | null>(null);

function extractCarFromKafkaEvent(evt: KafkaEventLike): LiveCar | null {
  const fromHeader = normalizeLiveCar(evt.headers?.car_type);
  if (fromHeader) return fromHeader;

  if (typeof evt.payload === "string") {
    try {
      const parsed = JSON.parse(evt.payload);
      const fromPayload = normalizeLiveCar(
        typeof parsed?.car_type === "string"
          ? parsed.car_type
          : typeof parsed?.carType === "string"
            ? parsed.carType
            : null,
      );
      if (fromPayload) return fromPayload;
    } catch {
      // ignore malformed payloads
    }
  }

  return null;
}

export function CarSelectionProvider({
  children,
}: {
  children: React.ReactNode;
}) {
  const [selectedCar, setSelectedCarState] = useState<LiveCar>(DEFAULT_CAR);

  useEffect(() => {
    if (typeof window === "undefined") return;
    let cancelled = false;

    if (!MULTI_CAR_ENABLED) {
      const bootstrapFromEvent = async () => {
        try {
          const res = await fetch("/api/event-active", { cache: "no-store" });
          if (!res.ok || cancelled) return;
          const payload: { carName?: string | null; car_name?: string | null } =
            await res.json();
          const car = normalizeLiveCar(payload.carName ?? payload.car_name);
          if (car && !cancelled) {
            setSelectedCarState(car);
          }
        } catch {
          // leave default
        }
      };
      bootstrapFromEvent();
      return () => {
        cancelled = true;
      };
    }

    const urlCar = normalizeLiveCar(
      new URLSearchParams(window.location.search).get("car"),
    );
    const persistedCar = normalizeLiveCar(localStorage.getItem(STORAGE_KEY));

    if (urlCar) {
      setSelectedCarState(urlCar);
      return;
    }

    if (persistedCar) {
      setSelectedCarState(persistedCar);
      return;
    }

    const bootstrapFromEvent = async () => {
      try {
        const res = await fetch("/api/event-active", { cache: "no-store" });
        if (!res.ok || cancelled) return;
        const payload: { carName?: string | null; car_name?: string | null } =
          await res.json();
        const car = normalizeLiveCar(payload.carName ?? payload.car_name);
        if (car && !cancelled) {
          setSelectedCarState(car);
        }
      } catch {
        // leave default
      }
    };

    bootstrapFromEvent();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (typeof window === "undefined") return;
    if (!MULTI_CAR_ENABLED) return;

    localStorage.setItem(STORAGE_KEY, selectedCar);

    const currentUrl = new URL(window.location.href);
    if (currentUrl.searchParams.get("car") === selectedCar) return;

    currentUrl.searchParams.set("car", selectedCar);
    window.history.replaceState({}, "", currentUrl.toString());
  }, [selectedCar]);

  useEffect(() => {
    if (typeof window === "undefined") return;
    if (!MULTI_CAR_ENABLED) return;

    const onKeyDown = (evt: KeyboardEvent) => {
      if (evt.key !== "[" && evt.key !== "]") return;
      const target = evt.target as HTMLElement | null;
      if (
        target &&
        (target.tagName === "INPUT" ||
          target.tagName === "TEXTAREA" ||
          target.isContentEditable)
      ) {
        return;
      }

      evt.preventDefault();
      const delta = evt.key === "]" ? 1 : -1;
      setSelectedCarState((prev) => {
        const idx = SUPPORTED_LIVE_CARS.indexOf(prev);
        const nextIdx =
          (idx + delta + SUPPORTED_LIVE_CARS.length) %
          SUPPORTED_LIVE_CARS.length;
        return SUPPORTED_LIVE_CARS[nextIdx] ?? prev;
      });
    };

    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, []);

  const setSelectedCar = useCallback((car: LiveCar) => {
    if (!MULTI_CAR_ENABLED) return;
    setSelectedCarState(car);
  }, []);

  const matchesSelectedCar = useCallback(
    (evt: KafkaEventLike) => {
      const eventCar = extractCarFromKafkaEvent(evt);
      if (!eventCar) return true;
      return eventCar === selectedCar;
    },
    [selectedCar],
  );

  const value = useMemo<CarSelectionContextValue>(
    () => ({
      selectedCar,
      selectedCarLabel: liveCarLabel(selectedCar),
      multiCarEnabled: MULTI_CAR_ENABLED,
      setSelectedCar,
      ssePath: `/api/kafka-stream?car=${selectedCar}&history=none`,
      matchesSelectedCar,
    }),
    [matchesSelectedCar, selectedCar, setSelectedCar],
  );

  return (
    <CarSelectionContext.Provider value={value}>
      {children}
    </CarSelectionContext.Provider>
  );
}

export function useCarSelection(): CarSelectionContextValue {
  const ctx = useContext(CarSelectionContext);
  if (!ctx) {
    throw new Error("useCarSelection must be used within CarSelectionProvider");
  }
  return ctx;
}
