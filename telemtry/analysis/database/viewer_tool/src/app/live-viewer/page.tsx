// Hidden for now: the live-viewer tile grid (3D car, GG plot, driver-input,
// thermal-headroom, shutdown-status) is parked while trackside-live is the one
// tool. Redirect keeps old links working; the original page + tile components
// remain in git history / the components dir to revisit and port later.
import { redirect } from "next/navigation";

export default function LiveViewerPage() {
  redirect("/trackside-live");
}
