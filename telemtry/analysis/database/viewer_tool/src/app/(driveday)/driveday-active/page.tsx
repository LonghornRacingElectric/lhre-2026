// Retired: drive-day lifecycle is handled inside /trackside-live now (starting a
// session creates the drive_day; Stop Event ends it). Redirect to the unified tool.
import { redirect } from "next/navigation";

export default function DrivedayActivePage() {
  redirect("/trackside-live");
}
