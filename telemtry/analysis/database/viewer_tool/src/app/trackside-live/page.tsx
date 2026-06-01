import type { Metadata } from "next";
import TracksideClient from "./TracksideClient";

export const metadata: Metadata = {
  title: "Trackside Live",
  description:
    "Live trackside telemetry dashboard (ported from MotecTelemetryExporter) wired to the LHR Kafka data streams.",
};

// The dashboard reads localStorage during initial state setup (ported from a
// client-only Vite app), so it must not server-render. TracksideClient is a
// client wrapper that dynamically imports it with SSR disabled.
export default function TracksideLivePage() {
  return <TracksideClient />;
}
