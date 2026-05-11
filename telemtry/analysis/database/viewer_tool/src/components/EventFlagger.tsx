"use client";

import { useEffect, useState } from "react";
import { toast } from "react-toastify";

import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";

const EVENT_FLAGS = {
  HIT_CONE: "hit cone",
  OFF_TRACK: "off track",
  MARK_INCOMPLETE: "incomplete",
  OTHER_FLAG: "other",
} as const;

type EventFlag = (typeof EVENT_FLAGS)[keyof typeof EVENT_FLAGS];

type Props = {
  note: string;
  setNote: (value: string) => void;
};

export default function EventFlagger({ note, setNote }: Props) {
  const [eventActive, setEventActive] = useState<boolean>(false);
  const disabledReason = "Can only be used when an event is active";

  useEffect(() => {
    let cancelled = false;

    const loadEventActive = async () => {
      try {
        const res = await fetch("/api/driveday-active", { cache: "no-store" });
        if (!res.ok) return;
        const data: { eventActive?: boolean } = await res.json();
        if (!cancelled) setEventActive(Boolean(data.eventActive));
      } catch {
        // ignore
      }
    };

    loadEventActive();
    return () => {
      cancelled = true;
    };
  }, []);

  const addFlag = async (eventFlag: EventFlag, maybeNote?: string) => {
    if (!eventActive) return;
    if (eventFlag === EVENT_FLAGS.OTHER_FLAG && (!maybeNote || maybeNote.trim() === "")) return;

    const response = await fetch("/api/event-flag", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        eventFlag: eventFlag === EVENT_FLAGS.OTHER_FLAG ? maybeNote : eventFlag,
      }),
    });

    if (response.ok) {
      if (eventFlag === EVENT_FLAGS.OTHER_FLAG) setNote("");
      toast("Flag added successfully", { type: "success" });
    } else {
      toast("Failed to add flag", { type: "error" });
    }
  };

  return (
    <div className="flex flex-col space-y-2">
      <span className="block w-full" title={!eventActive ? disabledReason : undefined}>
        <Button
          variant="outline"
          className="w-full bg-yellow-400 text-black hover:bg-yellow-500"
          onClick={() => addFlag(EVENT_FLAGS.HIT_CONE)}
          disabled={!eventActive}
        >
          Hit Cone
        </Button>
      </span>
      <span className="block w-full" title={!eventActive ? disabledReason : undefined}>
        <Button
          variant="outline"
          className="w-full bg-red-500 text-white hover:bg-red-600"
          onClick={() => addFlag(EVENT_FLAGS.OFF_TRACK)}
          disabled={!eventActive}
        >
          Off-track
        </Button>
      </span>
      <span className="block w-full" title={!eventActive ? disabledReason : undefined}>
        <Button
          variant="outline"
          className="w-full bg-orange-500 text-white hover:bg-orange-600"
          onClick={() => addFlag(EVENT_FLAGS.MARK_INCOMPLETE)}
          disabled={!eventActive}
        >
          Mark Incomplete
        </Button>
      </span>
      <div className="flex space-x-2 pt-2">
        <Input
          type="text"
          placeholder="Enter a note..."
          value={note}
          onChange={(e) => setNote(e.target.value)}
          disabled={!eventActive}
        />
        <span title={!eventActive ? disabledReason : undefined}>
          <Button
            onClick={() => addFlag(EVENT_FLAGS.OTHER_FLAG, note)}
            disabled={!eventActive || !note || note.trim() === ""}
          >
            Custom Flag
          </Button>
        </span>
      </div>
    </div>
  );
}
