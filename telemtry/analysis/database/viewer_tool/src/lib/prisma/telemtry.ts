// Dynamically choose the Prisma client (telemetry vs angelique) based on POSTGRES_DB
// This keeps dev hot-reload stable by using a global singleton.

import type { PrismaClient as TelemetryPrismaClient } from '../../../.prisma/telemetry-client';
import type { PrismaClient as AngeliquePrismaClient } from '../../../.prisma/angelique-client';

type AnyPrismaClient = TelemetryPrismaClient | AngeliquePrismaClient;

// Decide which client to use
const dbName = (process.env.POSTGRES_DB || '').toLowerCase();
const useAngelique = dbName === 'angelique';

// Require at runtime to avoid bundling issues in Next
const PrismaClientCtor: new () => AnyPrismaClient = useAngelique
  ? (require('../../../.prisma/angelique-client').PrismaClient as new () => AngeliquePrismaClient)
  : (require('../../../.prisma/telemetry-client').PrismaClient as new () => TelemetryPrismaClient);

declare global {
  // eslint-disable-next-line no-var
  var __unified_prisma__: AnyPrismaClient | undefined;
}

const prisma: AnyPrismaClient = global.__unified_prisma__ ?? new PrismaClientCtor();

if (process.env.NODE_ENV !== 'production') {
  global.__unified_prisma__ = prisma;
}

export default prisma;
