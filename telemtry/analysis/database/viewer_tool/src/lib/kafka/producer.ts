import { Producer } from "kafkajs";
import { getKafka } from "./kafka";

// Single shared KafkaJS producer (lazy, survives HMR via globalThis), used for
// app -> Kafka control messages such as live car_status threshold updates.
declare global {
  var __viewerToolKafkaProducer: Producer | undefined;
  var __viewerToolKafkaProducerReady: Promise<Producer> | undefined;
}

export async function getProducer(): Promise<Producer> {
  if (globalThis.__viewerToolKafkaProducerReady) {
    return globalThis.__viewerToolKafkaProducerReady;
  }
  globalThis.__viewerToolKafkaProducerReady = (async () => {
    try {
      const producer = getKafka().producer();
      await producer.connect();
      globalThis.__viewerToolKafkaProducer = producer;
      return producer;
    } catch (err) {
      // Don't cache a rejected promise — let the next call retry the connection.
      globalThis.__viewerToolKafkaProducerReady = undefined;
      throw err;
    }
  })();
  return globalThis.__viewerToolKafkaProducerReady;
}

/** Publish a JSON value to a topic. */
export async function publishJson(topic: string, value: unknown): Promise<void> {
  const producer = await getProducer();
  await producer.send({ topic, messages: [{ value: JSON.stringify(value) }] });
}
