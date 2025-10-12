import prisma from '../src/lib/prisma/telemtry';
import dotenv from 'dotenv';
import path from 'path';

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

async function main() {
  // Create dummy drive_day

  // Create dummy car
  const car = await prisma.lut_car.upsert({
    where: { car_id: 9999 },
    update: {},
    create: { car_id: 9999, car_name: 'Dummy Car' }, // adjust fields
  });

  // Create dummy driver
  const driver = await prisma.lut_driver.upsert({
    where: { driver_id: 9999 },
    update: {},
    create: { driver_id: 9999, driver_name: 'Dummy Driver' },
  });

  // Create dummy location
  const location = await prisma.lut_location.upsert({
    where: { location_id: 9999 },
    update: {},
    create: { location_id: 9999, area: 'Dummy Area', track: 'Dummy Track' },
  });

  // Create dummy event type
  const eventType = await prisma.lut_event_type.upsert({
    where: { type_id: 9999 },
    update: {},
    create: { type_id: 9999, event_type: 'Dummy Event Type' }, // adjust fields
  });

  console.log('Seeded event with ID 9999 and all dummy foreign keys.');
}

main()
  .catch((e) => console.error(e))
  .finally(async () => {
    await prisma.$disconnect();
  });
