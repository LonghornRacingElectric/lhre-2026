import prismaAngelique from "@/lib/prisma/angelique";
import prismaTelemtry from "@/lib/prisma/telemtry";

export type SupportedCar = "angelique" | "orion";

export function normalizeCar(value: string | null | undefined): SupportedCar | null {
  if (!value) return null;
  const normalized = value.trim().toLowerCase();
  if (normalized === "angelique") return "angelique";
  if (normalized === "orion") return "orion";
  return null;
}

export function getCarPrisma(car: SupportedCar | null | undefined) {
  if (car === "angelique") return prismaAngelique;
  return prismaTelemtry;
}

export async function resolveCarFromCarId(
  carId: number | null | undefined,
): Promise<SupportedCar | null> {
  if (carId == null) return null;
  const lut = await prismaTelemtry.lut_car.findUnique({
    where: { car_id: carId },
    select: { car_name: true },
  });
  return normalizeCar(lut?.car_name);
}

export async function resolveCarFromEvent(
  eventId: number,
): Promise<SupportedCar | null> {
  const event = await prismaTelemtry.event.findUnique({
    where: { event_id: eventId },
    select: {
      car_id: true,
      car: { select: { car_name: true } },
    },
  });
  if (!event) return null;
  return normalizeCar(event.car?.car_name) ?? (await resolveCarFromCarId(event.car_id));
}
