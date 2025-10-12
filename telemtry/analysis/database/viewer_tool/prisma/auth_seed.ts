import { PrismaClient } from '../.prisma/auth-client';
import { hash } from 'bcrypt';

const prisma = new PrismaClient();

async function main() {
  const password = await hash('2quick2fast', 12);
  const user = await prisma.user.upsert({
    where: { username: 'ELC' },
    update: {},
    create: {
      username: 'ELC',
      name: 'ELC',
      password,
    },
  });
  console.log({ user });
}

main()
  .then(() => prisma.$disconnect())
  .catch(async (e) => {
    console.error(e);
    await prisma.$disconnect();
    process.exit(1);
  });