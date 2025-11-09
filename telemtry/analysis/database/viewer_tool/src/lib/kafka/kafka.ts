import { Kafka, logLevel } from "kafkajs";

let kafka: Kafka | null = null;

export function getKafka(): Kafka {
  if (!kafka) {
    const brokersRaw = process.env.KAFKA_BROKERS || "";
    const brokers = brokersRaw.split(",").map(b => b.trim()).filter(Boolean);
    if (brokers.length === 0) {
      throw new Error("KAFKA_BROKERS env var is required (comma-separated)");
    }
    kafka = new Kafka({
      clientId: process.env.KAFKA_CLIENT_ID || "viewer-tool",
      brokers,
      logLevel: logLevel.NOTHING,
    });
  }
  return kafka;
}
