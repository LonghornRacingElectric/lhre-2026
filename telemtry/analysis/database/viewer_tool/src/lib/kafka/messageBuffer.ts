import { KafkaEvent } from "./bus";

const g = globalThis as any;
const DEFAULT_BUFFER_SIZE = 200;
const configuredSize = Number(process.env.KAFKA_BUFFER_SIZE ?? DEFAULT_BUFFER_SIZE);
const BUFFER_SIZE =
  Number.isFinite(configuredSize) && configuredSize > 0
    ? Math.floor(configuredSize)
    : DEFAULT_BUFFER_SIZE;

// Map topic -> array of events
const buffers = g.__kafkaBuffers || new Map<string, KafkaEvent[]>();
g.__kafkaBuffers = buffers;

export function bufferMessage(topic: string, event: KafkaEvent) {
  if (!buffers.has(topic)) {
    buffers.set(topic, []);
  }
  const arr = buffers.get(topic)!;
  arr.push(event);
  
  if (arr.length > BUFFER_SIZE) {
    arr.splice(0, arr.length - BUFFER_SIZE);
  }
}

export function getBufferedMessages(topic: string): KafkaEvent[] {
  return buffers.get(topic) || [];
}

export function clearBuffer(topic: string) {
  if (buffers.has(topic)) {
    buffers.set(topic, []);
  }
}

export function clearAllBuffers() {
  buffers.clear();
}
