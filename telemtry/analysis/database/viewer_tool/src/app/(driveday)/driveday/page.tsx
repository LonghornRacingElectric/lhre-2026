// Retired: the Driveday "New Drive Day" setup form is now part of /trackside-live
// (a session IS a drive day — see the "Drive Day Setup" modal there). Kept as a
// redirect so old links/bookmarks land on the unified tool. The drive-day APIs
// (/api/new-drive-day, /api/update-drive-day, /api/end-event) are still used by
// trackside-live.
import { redirect } from "next/navigation";

export default function DrivedayPage() {
  redirect("/trackside-live");
}
