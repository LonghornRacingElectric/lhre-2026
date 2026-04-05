import { KafkaEvent } from "./bus";

const g = globalThis as any;
const BUFFER_SIZE = 1000; // Keep last 1000 messages per topic by default

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
    arr.shift();
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
