// Port of motec_ld.py — MoTeC i2 .ld binary writer + .ldx beacon XML.
// Byte-for-byte faithful to the reference: same pointer offsets and struct layouts,
// implemented with a minimal Python-`struct`-compatible packer over Node Buffers.

import { Channel, DataLog } from "./datalog";

const VEHICLE_PTR = 1762;
const VENUE_PTR = 5078;
const EVENT_PTR = 8180;
const HEADER_PTR = 11336;

const HEAD_FMT =
  "<I4xII20xI24xHHHI8sHHI4x16s16x16s16x64s64s64x64s64x1024xI66x64s126x64s64s";
const VEHICLE_FMT = "<64s128xI32s32s";
const VENUE_FMT = "<64s1034xH";
const EVENT_FMT = "<64s64s1024sH";
const CHAN_FMT = "<IIIIHHHHHHHh32s8s12s40x";

type PackValue = number | string;

type Token =
  | { kind: "pad"; count: number }
  | { kind: "str"; size: number }
  | { kind: "num"; type: "I" | "i" | "H" | "h" | "B" | "b"; count: number };

function parseFormat(fmt: string): Token[] {
  const body = fmt.startsWith("<") || fmt.startsWith(">") ? fmt.slice(1) : fmt;
  const tokens: Token[] = [];
  let i = 0;
  while (i < body.length) {
    let numStr = "";
    while (i < body.length && body[i] >= "0" && body[i] <= "9") {
      numStr += body[i];
      i += 1;
    }
    const type = body[i];
    i += 1;
    const count = numStr === "" ? 1 : parseInt(numStr, 10);
    if (type === "x") tokens.push({ kind: "pad", count });
    else if (type === "s") tokens.push({ kind: "str", size: count });
    else tokens.push({ kind: "num", type: type as "I" | "i" | "H" | "h" | "B" | "b", count });
  }
  return tokens;
}

function sizeOf(tokens: Token[]): number {
  const widths: Record<string, number> = { I: 4, i: 4, H: 2, h: 2, B: 1, b: 1 };
  let size = 0;
  for (const tok of tokens) {
    if (tok.kind === "pad") size += tok.count;
    else if (tok.kind === "str") size += tok.size;
    else size += widths[tok.type] * tok.count;
  }
  return size;
}

// Little-endian pack matching Python struct semantics for the subset used here.
function pack(fmt: string, values: PackValue[]): Buffer {
  const tokens = parseFormat(fmt);
  const buf = Buffer.alloc(sizeOf(tokens));
  let offset = 0;
  let vi = 0;
  for (const tok of tokens) {
    if (tok.kind === "pad") {
      offset += tok.count; // already zero
    } else if (tok.kind === "str") {
      const raw = String(values[vi++] ?? "");
      const encoded = Buffer.from(raw, "ascii");
      const n = Math.min(encoded.length, tok.size);
      encoded.copy(buf, offset, 0, n);
      offset += tok.size;
    } else {
      for (let c = 0; c < tok.count; c++) {
        const v = Number(values[vi++] ?? 0);
        switch (tok.type) {
          case "I": buf.writeUInt32LE(v >>> 0, offset); offset += 4; break;
          case "i": buf.writeInt32LE(v | 0, offset); offset += 4; break;
          case "H": buf.writeUInt16LE(v & 0xffff, offset); offset += 2; break;
          case "h": buf.writeInt16LE(((v << 16) >> 16), offset); offset += 2; break;
          case "B": buf.writeUInt8(v & 0xff, offset); offset += 1; break;
          case "b": buf.writeInt8(((v << 24) >> 24), offset); offset += 1; break;
        }
      }
    }
  }
  return buf;
}

const CHANNEL_HEADER_SIZE = sizeOf(parseFormat(CHAN_FMT)); // 124

function fixed(text: string, size: number): string {
  // Mirror _fixed: ascii, drop non-ascii, truncate to size-1 when too long.
  const ascii = (text ?? "").toString().replace(/[^\x00-\x7F]/g, "");
  return ascii.length >= size ? ascii.slice(0, size - 1) : ascii;
}

function cleanFloat(value: number): number {
  if (!Number.isFinite(value)) return 0;
  return Math.max(-3.4e38, Math.min(3.4e38, value));
}

function pad2(n: number): string {
  return String(n).padStart(2, "0");
}

function metadataDate(metadata: Record<string, string>): Date {
  const raw = metadata.datetime;
  if (raw) {
    const parsed = new Date(raw);
    if (!Number.isNaN(parsed.getTime())) return parsed;
  }
  return new Date();
}

export function writeLd(dataLog: DataLog, metadata: Record<string, string> = {}): Buffer {
  const channels: Channel[] = [...dataLog.channels.values()].filter((c) => c.samples.length);

  const channelHeaders: Buffer[] = [];
  const channelData: Buffer[] = [];
  let metaPtr = HEADER_PTR;
  let dataPtr = HEADER_PTR + channels.length * CHANNEL_HEADER_SIZE;

  channels.forEach((channel, index) => {
    const values = Buffer.alloc(channel.samples.length * 4);
    channel.samples.forEach((s, i) => values.writeFloatLE(cleanFloat(s.value), i * 4));
    const prevMeta = index === 0 ? 0 : metaPtr - CHANNEL_HEADER_SIZE;
    const nextMeta = index === channels.length - 1 ? 0 : metaPtr + CHANNEL_HEADER_SIZE;
    const freq = Math.max(1, Math.round(channel.averageFrequency || 1));
    channelHeaders.push(
      pack(CHAN_FMT, [
        prevMeta,
        nextMeta,
        dataPtr,
        channel.samples.length,
        0x2ee1 + index,
        0x07,
        4,
        freq,
        0,
        1,
        1,
        0,
        fixed(channel.name, 32),
        fixed(channel.quantity, 8),
        fixed(channel.unit, 12),
      ]),
    );
    metaPtr += CHANNEL_HEADER_SIZE;
    dataPtr += values.length;
    channelData.push(values);
  });

  const d = metadataDate(metadata);
  const dateStr = `${pad2(d.getUTCDate())}/${pad2(d.getUTCMonth() + 1)}/${d.getUTCFullYear()}`;
  const timeStr = `${pad2(d.getUTCHours())}:${pad2(d.getUTCMinutes())}:${pad2(d.getUTCSeconds())}`;

  const header = pack(HEAD_FMT, [
    0x40,
    HEADER_PTR,
    HEADER_PTR + channels.length * CHANNEL_HEADER_SIZE,
    EVENT_PTR,
    1,
    0x4240,
    0xf,
    0x1f44,
    "ADL",
    420,
    0xadb0,
    channels.length,
    fixed(dateStr, 16),
    fixed(timeStr, 16),
    fixed(metadata.driver ?? "", 64),
    fixed(metadata.vehicle_id ?? "Orion", 64),
    fixed(metadata.venue ?? "", 64),
    0xc81a4,
    fixed(metadata.short_comment ?? dataLog.name, 64),
    fixed(metadata.event ?? "Telemetry Export", 64),
    fixed(metadata.session ?? "", 64),
  ]);

  const vehicle = pack(VEHICLE_FMT, [
    fixed(metadata.vehicle_id ?? "Orion", 64),
    parseInt(metadata.vehicle_weight ?? "0", 10) || 0,
    fixed(metadata.vehicle_type ?? "EV", 32),
    fixed(metadata.vehicle_comment ?? "", 32),
  ]);
  const venue = pack(VENUE_FMT, [fixed(metadata.venue ?? "", 64), VEHICLE_PTR]);
  const event = pack(EVENT_FMT, [
    fixed(metadata.event ?? "Telemetry Export", 64),
    fixed(metadata.session ?? "", 64),
    fixed(metadata.long_comment ?? "", 1024),
    VENUE_PTR,
  ]);

  const totalData = channelData.reduce((sum, b) => sum + b.length, 0);
  const total = HEADER_PTR + channels.length * CHANNEL_HEADER_SIZE + totalData;
  const file = Buffer.alloc(total);

  header.copy(file, 0);
  vehicle.copy(file, VEHICLE_PTR);
  venue.copy(file, VENUE_PTR);
  event.copy(file, EVENT_PTR);
  let cursor = HEADER_PTR;
  for (const h of channelHeaders) {
    h.copy(file, cursor);
    cursor += h.length;
  }
  for (const dbuf of channelData) {
    dbuf.copy(file, cursor);
    cursor += dbuf.length;
  }
  return file;
}

function normalTimes(values: number[]): number[] {
  const out: number[] = [];
  let previous: number | null = null;
  for (const value of [...values].map(Number).sort((a, b) => a - b)) {
    if (previous !== null && value - previous < 0.001) continue;
    out.push(value);
    previous = value;
  }
  return out;
}

function xmlEscape(s: string): string {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

export function writeLdx(primaryBeaconsS: number[], splitBeaconsS: number[] = []): string {
  const markers: Array<[string, string, number]> = [];
  normalTimes(primaryBeaconsS).forEach((seconds, i) => markers.push(["BCN", `Manual.${i + 1}`, seconds]));
  normalTimes(splitBeaconsS).forEach((seconds, i) => markers.push(["SPLTBCN", `Split.${i + 1}`, seconds]));
  markers.sort((a, b) => a[2] - b[2] || (a[0] === "BCN" ? 0 : 1) - (b[0] === "BCN" ? 0 : 1));

  const totalLaps = normalTimes(primaryBeaconsS).length + 1;
  const markerXml = markers
    .map(
      ([className, name, seconds]) =>
        `        <Marker Version="100" ClassName="${className}" Name="${xmlEscape(name)}" Flags="77" Time="${(seconds * 1_000_000).toFixed(6)}" />`,
    )
    .join("\n");

  return `<?xml version='1.0' encoding='utf-8'?>
<LDXFile Version="1.6" Locale="English">
  <Layers>
    <Layer>
      <MarkerBlock>
        <MarkerGroup Name="Beacons" Index="3">
${markerXml}
        </MarkerGroup>
      </MarkerBlock>
      <RangeBlock />
      <Details>
        <String Id="Total Laps" Value="${totalLaps}" />
      </Details>
    </Layer>
  </Layers>
</LDXFile>
`;
}
