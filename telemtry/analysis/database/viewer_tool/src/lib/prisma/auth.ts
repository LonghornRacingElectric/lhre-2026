
import { PrismaClient } from '../../../.prisma/auth-client';

declare global {
  var auth_prisma: PrismaClient | undefined;
}

if (!global.auth_prisma) {
  global.auth_prisma = new PrismaClient();
}

var auth_prisma: PrismaClient = global.auth_prisma!;


export default auth_prisma;
