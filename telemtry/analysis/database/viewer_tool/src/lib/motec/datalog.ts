// Port of datalog.py — Sample / Channel / DataLog with extract + resample.

export type Sample = { t: number; value: number };

export class Channel {
  name: string;
  unit: string;
  quantity: string;
  samples: Sample[];

  constructor(name: string, unit = "", quantity = "", samples: Sample[] = []) {
    this.name = name;
    this.unit = unit;
    this.quantity = quantity;
    this.samples = samples;
  }

  get start(): number {
    return this.samples.length ? this.samples[0].t : 0;
  }

  get end(): number {
    return this.samples.length ? this.samples[this.samples.length - 1].t : 0;
  }

  get averageFrequency(): number {
    if (this.samples.length < 2 || this.end <= this.start) return 0;
    return (this.samples.length - 1) / (this.end - this.start);
  }

  extract(start: number, end: number, rebase = true): Channel {
    const offset = rebase ? start : 0;
    return new Channel(
      this.name,
      this.unit,
      this.quantity,
      this.samples
        .filter((s) => s.t >= start && s.t <= end && Number.isFinite(s.value))
        .map((s) => ({ t: s.t - offset, value: s.value })),
    );
  }

  resample(start: number, end: number, frequencyHz: number | null): void {
    if (!this.samples.length) return;
    const targetHz = frequencyHz || Math.max(1, Math.round(this.averageFrequency || 20));
    if (targetHz <= 0 || end <= start) return;
    const step = 1 / targetHz;
    const count = Math.max(1, Math.floor((end - start) * targetHz) + 1);
    const source = this.samples;
    const out: Sample[] = [];
    let index = 0;
    for (let n = 0; n < count; n++) {
      const t = Math.min(end, start + n * step);
      let value: number;
      if (t <= source[0].t) {
        value = source[0].value;
      } else if (t >= source[source.length - 1].t) {
        value = source[source.length - 1].value;
      } else {
        while (index + 1 < source.length && source[index + 1].t < t) index += 1;
        const a = source[index];
        const b = source[index + 1];
        if (b.t <= a.t) value = a.value;
        else value = a.value + (b.value - a.value) * ((t - a.t) / (b.t - a.t));
      }
      out.push({ t, value });
    }
    this.samples = out;
  }
}

export class DataLog {
  name: string;
  channels: Map<string, Channel>;
  metadata: Record<string, string>;

  constructor(name: string, metadata: Record<string, string> = {}) {
    this.name = name;
    this.channels = new Map();
    this.metadata = metadata;
  }

  get start(): number {
    const starts = [...this.channels.values()].filter((c) => c.samples.length).map((c) => c.start);
    return starts.length ? Math.min(...starts) : 0;
  }

  get end(): number {
    const ends = [...this.channels.values()].filter((c) => c.samples.length).map((c) => c.end);
    return ends.length ? Math.max(...ends) : 0;
  }

  resample(frequencyHz: number | null): void {
    const start = this.start;
    const end = this.end;
    for (const channel of this.channels.values()) channel.resample(start, end, frequencyHz);
  }
}
