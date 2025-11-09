import { EventEmitter } from "events";

// Global process-wide event bus (survives HMR by stashing on globalThis)
const g = globalThis as any;
export const bus: EventEmitter = g.__viewerToolEventBus || new EventEmitter();
bus.setMaxListeners(0);
g.__viewerToolEventBus = bus;

export type KafkaEvent = {
  topic: string;
  partition: number;
  payload: string;
  headers?: Record<string, string | undefined>;
  offset: string;
  timestamp: string;
};
