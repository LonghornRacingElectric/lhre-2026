// Port of db.py — read-only Postgres access for the MoTeC exporter.
// Enforces the safety contract: SELECT/CTE only, read-only transaction, statement timeout.

import { Pool, type PoolClient } from "pg";
import type { Settings } from "./config";

const SELECT_RE = /^\s*(select|with)\b/i;
const FORBIDDEN_RE = /\b(insert|update|delete|merge|alter|drop|create|truncate|grant|revoke|vacuum|copy\s+[^()]*\s+from)\b/i;

// Pools keyed by connection identity so we don't reconnect per query.
const pools = new Map<string, Pool>();

function poolFor(settings: Settings): Pool {
  const key = `${settings.orionDbHost}:${settings.orionDbPort}/${settings.orionDbName}/${settings.orionDbUser}`;
  let pool = pools.get(key);
  if (!pool) {
    pool = new Pool({
      host: settings.orionDbHost,
      port: settings.orionDbPort,
      database: settings.orionDbName,
      user: settings.orionDbUser,
      password: settings.orionDbPassword,
      ssl: settings.orionDbSslmode && settings.orionDbSslmode !== "disable" ? { rejectUnauthorized: false } : undefined,
      connectionTimeoutMillis: settings.orionDbConnectTimeout * 1000,
      max: 4,
    });
    pools.set(key, pool);
  }
  return pool;
}

function assertReadOnly(sql: string): void {
  if (!SELECT_RE.test(sql)) throw new Error("Only SELECT/CTE queries are allowed.");
  if (FORBIDDEN_RE.test(sql)) throw new Error("Query contains a forbidden mutating keyword.");
}

export class ReadOnlyDatabase {
  constructor(private settings: Settings) {}

  async query<T = Record<string, unknown>>(sql: string, params: unknown[] = []): Promise<T[]> {
    assertReadOnly(sql);
    if (!this.settings.orionDbPassword) throw new Error("ORION_DB_PASSWORD is not set.");
    const pool = poolFor(this.settings);
    const client: PoolClient = await pool.connect();
    try {
      await client.query("BEGIN TRANSACTION READ ONLY");
      await client.query("SET LOCAL statement_timeout = 30000");
      const res = await client.query(sql, params);
      await client.query("COMMIT");
      return res.rows as T[];
    } catch (err) {
      try { await client.query("ROLLBACK"); } catch { /* ignore */ }
      throw err;
    } finally {
      client.release();
    }
  }
}
