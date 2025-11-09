import { PrismaClient } from '../.prisma/auth-client';
import { hash } from 'bcrypt';
import * as dotenv from 'dotenv';
import * as path from 'path';

dotenv.config({ path: path.resolve(__dirname, '../../../../.env') });

const prisma = new PrismaClient();

async function main() {
  const username = process.argv[2];
  const password = process.argv[3];

  if (!username || !password) {
    console.error('Please provide both a username and a password.');
    process.exit(1);
  }

  const hashedPassword = await hash(password, 12);
  const user = await prisma.user.create({
    data: {
      username,
      name: username,
      password: hashedPassword,
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