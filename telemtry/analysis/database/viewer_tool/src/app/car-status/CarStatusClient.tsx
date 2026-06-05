"use client";

import dynamic from "next/dynamic";

// Client-only mount: the page holds live-stream + slider state and uses
// browser APIs, so it must not server-render (same pattern as trackside-live).
const CarStatusApp = dynamic(() => import("./CarStatusApp"), {
  ssr: false,
  loading: () => (
    <div className="csLoading">
      <span className="csSpinner" aria-hidden="true" />
      Loading car status…
    </div>
  ),
});

export default function CarStatusClient() {
  return <CarStatusApp />;
}
