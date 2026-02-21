import { Pool } from 'pg';

declare global {
  var pool: Pool | undefined;
}

const database = {
  username: "electric",
  password: process.env.ELECTRIC_PWD,
  host: "localhost",
  port: 5432,
  dbName: "telemetry",
}

if (!global.pool) {
  const connectionString =
    process.env.TELEMETRY_DATABASE_URL ??
    `postgresql://${database.username}:${database.password}@${database.host}:${database.port}/${database.dbName}`;

  global.pool = new Pool({
    connectionString,
  });
}

export default global.pool!;
