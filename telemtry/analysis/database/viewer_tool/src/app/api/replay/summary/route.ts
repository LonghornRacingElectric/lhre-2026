import { NextRequest, NextResponse } from "next/server";
import prisma from "@/lib/prisma/telemtry";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

function toBigInt(value: string | null): bigint | null {
	if (value == null || value === "") return null;
	try {
		return BigInt(value);
	} catch {
		return null;
	}
}

function median(values: bigint[]): bigint | null {
	if (!values.length) return null;
	const sorted = [...values].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
	return sorted[Math.floor(sorted.length / 2)] ?? null;
}

// Returns the number of raw time units per millisecond.
async function inferRawUnitsPerMs(packetStart: bigint, packetEnd: bigint): Promise<bigint> {
	const sample = await prisma.packet.findMany({
		where: { packet_id: { gte: packetStart, lte: packetEnd } },
		orderBy: { packet_id: "asc" },
		take: 25,
		select: { time: true },
	});

	const times: bigint[] = sample
		.map((p) => (p.time != null ? BigInt(p.time as any) : null))
		.filter((t): t is bigint => t != null);

	const deltas: bigint[] = [];
	for (let i = 1; i < times.length; i++) {
		const dt = times[i]! - times[i - 1]!;
		if (dt > BigInt(0)) deltas.push(dt);
	}

	const med = median(deltas);
	if (med != null) {
		if (med >= BigInt(1000000)) return BigInt(1000000); // ns -> ms
		if (med >= BigInt(1000)) return BigInt(1000); // us -> ms
		return BigInt(1); // ms
	}

	// Fallback: magnitude-based.
	const t0 = times[0] ?? BigInt(0);
	if (t0 >= BigInt("10000000000000000")) return BigInt(1000000);
	if (t0 >= BigInt("10000000000000")) return BigInt(1000);
	return BigInt(1);
}

function bigintToSafeNumber(v: bigint | null): number | null {
	if (v == null) return null;
	const n = Number(v);
	if (!Number.isFinite(n)) return null;
	if (Math.abs(n) > Number.MAX_SAFE_INTEGER) return null;
	return n;
}

export async function GET(req: NextRequest) {
	try {
		const url = new URL(req.url);
		const eventIdRaw = url.searchParams.get("eventId");
		const eventId = eventIdRaw ? Number(eventIdRaw) : NaN;
		if (!Number.isFinite(eventId)) {
			return NextResponse.json({ error: "Missing/invalid 'eventId'" }, { status: 400 });
		}

		const ev = await prisma.event.findUnique({
			where: { event_id: eventId },
			select: { event_id: true, packet_start: true, packet_end: true },
		});
		if (!ev) return NextResponse.json({ error: "Event not found" }, { status: 404 });
		if (ev.packet_start == null || ev.packet_end == null) {
			return NextResponse.json({ error: "Event has no packet range" }, { status: 409 });
		}

		const packetStart = BigInt(ev.packet_start as any);
		const packetEnd = BigInt(ev.packet_end as any);

		const [p0, p1] = await Promise.all([
			prisma.packet.findUnique({ where: { packet_id: packetStart }, select: { time: true } }),
			prisma.packet.findUnique({ where: { packet_id: packetEnd }, select: { time: true } }),
		]);

		const startRaw = p0?.time != null ? BigInt(p0.time as any) : null;
		const endRaw = p1?.time != null ? BigInt(p1.time as any) : null;

		const rawUnitsPerMs = await inferRawUnitsPerMs(packetStart, packetEnd);

		const startMs = startRaw != null ? startRaw / rawUnitsPerMs : null;
		const endMs = endRaw != null ? endRaw / rawUnitsPerMs : null;

		const classifiers = await prisma.classifier.findMany({
			where: { event_id: eventId },
			orderBy: [{ start_time: "asc" }],
			select: { type: true, start_time: true, end_time: true, notes: true },
		});

		const lap_times: Array<{ start_time: string; end_time: string | null; notes: string | null }> = [];
		const flagged_events: Array<{ type: string; start_time: string; end_time: string | null; notes: string | null }> = [];

		for (const c of classifiers as any[]) {
			const tRaw = c.start_time != null ? BigInt(c.start_time as any) : null;
			const tEndRaw = c.end_time != null ? BigInt(c.end_time as any) : null;

			const inPacketRange =
				startRaw != null &&
				endRaw != null &&
				tRaw != null &&
				tRaw >= startRaw &&
				tRaw <= endRaw;

			const markerStartMs = inPacketRange && tRaw != null ? tRaw / rawUnitsPerMs : tRaw;
			const markerEndMs =
				inPacketRange && tEndRaw != null ? tEndRaw / rawUnitsPerMs : tEndRaw;

			const startNum = bigintToSafeNumber(markerStartMs);
			const endNum = bigintToSafeNumber(markerEndMs);
			if (startNum == null) continue;

			if (c.type === "lap") {
				lap_times.push({
					start_time: String(startNum),
					end_time: endNum != null ? String(endNum) : null,
					notes: c.notes ?? null,
				});
			} else {
				flagged_events.push({
					type: String(c.type),
					start_time: String(startNum),
					end_time: endNum != null ? String(endNum) : null,
					notes: c.notes ?? null,
				});
			}
		}

		return NextResponse.json(
			{
				event_id: ev.event_id,
				packet_start: packetStart.toString(),
				packet_end: packetEnd.toString(),
				time_start: bigintToSafeNumber(startMs),
				time_end: bigintToSafeNumber(endMs),
				lap_times,
				flagged_events,
			},
			{ status: 200 }
		);
	} catch (e) {
		console.error("Failed to load replay summary", e);
		return NextResponse.json({ error: "Failed to load replay summary" }, { status: 500 });
	}
}

