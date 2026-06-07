import type { Metadata } from "next";
import CarStatusClient from "./CarStatusClient";

export const metadata: Metadata = {
  title: "Car Status",
  description:
    "Central live + historical car state (Off / Idle / Ready / Moving / Fault) with HV SoC and LV battery voltage.",
};

export default function CarStatusPage() {
  return <CarStatusClient />;
}
