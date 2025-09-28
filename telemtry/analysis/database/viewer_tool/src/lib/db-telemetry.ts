import { Pool } from 'pg';

let pool;

declare global {
  var pool: Pool | undefined;
}

if (process.env.NODE_ENV === 'production') {
  pool = new Pool({
    connectionString: process.env.TELEMETRY_DATABASE_URL,
  });
} else {
  if (!global.pool) {
    global.pool = new Pool({
      host: 'localhost',
      port: 5432,
      database: 'telemetry',
      user: 'electric',
      password: process.env.ELECTRIC_PWD,
    });
  }
  pool = global.pool;
}

export default pool;
