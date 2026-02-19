// prisma-env.js
const { spawn } = require("child_process");

import path from 'path';
import dotenv from 'dotenv';

// Load .env from one level up
dotenv.config({ path: path.resolve(__dirname, '../../../../.env') });

const {
  POSTGRES_USER,
  POSTGRES_PASSWORD,
  POSTGRES_HOST,
  POSTGRES_PORT,
  POSTGRES_DB,
} = process.env;

if (POSTGRES_USER && POSTGRES_PASSWORD && POSTGRES_DB) {
  process.env.TELEMETRY_DATABASE_URL = `postgresql://${POSTGRES_USER}:${POSTGRES_PASSWORD}@${POSTGRES_HOST}:${POSTGRES_PORT}/${POSTGRES_DB}?schema=public`;
} else {
  console.warn(
    "⚠️ Missing database env vars: POSTGRES_USER, POSTGRES_PASSWORD, or POSTGRES_DB"
  );
}

// Run Prisma Studio
const cmd = spawn("npx", ["prisma", "studio", "--schema", ".prisma/angelique-client"], {
  stdio: "inherit",
  env: process.env,
});

cmd.on("exit", (code: any) => process.exit(code));
