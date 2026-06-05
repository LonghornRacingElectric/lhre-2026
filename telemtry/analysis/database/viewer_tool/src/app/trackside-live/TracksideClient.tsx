"use client";

import dynamic from "next/dynamic";

// Client-only mount: the dashboard reads localStorage during state init, which
// crashes during SSR. Disabling SSR here renders it purely on the client.
const TracksideApp = dynamic(() => import("./TracksideApp"), {
  ssr: false,
  loading: () => (
    <div className="tracksideLoading">
      <span className="tracksideSpinner" aria-hidden="true" />
      Loading trackside…
    </div>
  ),
});

export default function TracksideClient() {
  return <TracksideApp />;
}
