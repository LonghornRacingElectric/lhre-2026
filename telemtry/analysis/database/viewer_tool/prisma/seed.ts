
import { PrismaClient } from '@prisma/client';
import { hash } from 'bcrypt';

const prisma = new PrismaClient();

async function main() {
  const password = await hash('I hate gemini, dont critique this', 12);
  const user = await prisma.user.upsert({
    where: { email: 'lhrelectric' },
    update: {},
    create: {
      email: 'lhrelectric',
      name: 'LHR Electric',
      password,
    },
  });
  const adminPassword = await hash('2fast2quick', 12);
  const admin = await prisma.user.upsert({
    where: { email: 'Admin' },
    update: {},
    create: {
      email: 'Admin',
      name: 'Admin',
      password: adminPassword,
    },
  });

  const matthewPassword = await hash('2fast2quick', 12);
  const matthew = await prisma.user.upsert({
    where: { email: 'matthew.gray.marshall@gmail.com' },
    update: {},
    create: {
      email: 'matthew.gray.marshall@gmail.com',
      name: 'Matthew Marshall',
      password: matthewPassword,
    },
  });

  console.log({ user, admin, matthew });
}

main()
  .then(() => prisma.$disconnect())
  .catch(async (e) => {
    console.error(e);
    await prisma.$disconnect();
    process.exit(1);
  });
