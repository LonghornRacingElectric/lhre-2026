import { PrismaClient } from "../../../.prisma/angelique-client";

declare global {
  var angelique_prisma: PrismaClient | undefined;
}

if (!global.angelique_prisma) {
  global.angelique_prisma = new PrismaClient();
}

const angelique_prisma: PrismaClient = global.angelique_prisma;

export default angelique_prisma;
