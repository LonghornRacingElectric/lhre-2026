import { PrismaClient } from "../../../.prisma/telemetry-client";

declare global {
  var telemtry_prisma: PrismaClient | undefined;
}

if (!global.telemtry_prisma) {
  global.telemtry_prisma = new PrismaClient();
}

const telemtry_prisma: PrismaClient = global.telemtry_prisma!;


export default telemtry_prisma;
