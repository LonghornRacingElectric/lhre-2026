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
  global.pool = new Pool({
    connectionString: `postgresql://${database.username}:${database.password}@${database.host}:${database.port}/${database.dbName}`
  });
}

export default global.pool;
