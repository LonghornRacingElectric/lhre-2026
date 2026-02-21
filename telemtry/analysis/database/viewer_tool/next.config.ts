import type { NextConfig } from "next";

import path from 'path';
import dotenv from 'dotenv';

// Load .env from one level up
dotenv.config({ path: path.resolve(__dirname, '../../../.env') });

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

const nextConfig: NextConfig = {
  /* config options here */
  compress: false, // Disable compression to prevent buffering of SSE
};

export default nextConfig;
