import prismaAngelique from "@/lib/prisma/angelique";
import prismaTelemtry from "@/lib/prisma/telemtry";

export type SupportedCar = "angelique" | "orion";
export type PacketScalar = bigint | number;
export type ReplayPacket = {
  packet_id: PacketScalar;
  time: PacketScalar | null;
  dynamics: Record<string, unknown> | null;
  controls: Record<string, unknown> | null;
};

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

export async function findLatestPacketId(
  car: SupportedCar | null | undefined,
): Promise<PacketScalar | null> {
  if (car === "angelique") {
    const latest = await prismaAngelique.packet.findFirst({
      orderBy: { packet_id: "desc" },
      select: { packet_id: true },
    });
    return latest?.packet_id ?? null;
  }

  const latest = await prismaTelemtry.packet.findFirst({
    orderBy: { packet_id: "desc" },
    select: { packet_id: true },
  });
  return latest?.packet_id ?? null;
}

export async function findPacketTimeById(
  car: SupportedCar | null | undefined,
  packetId: PacketScalar,
): Promise<PacketScalar | null> {
  if (car === "angelique") {
    const packet = await prismaAngelique.packet.findUnique({
      where: { packet_id: packetId },
      select: { time: true },
    });
    return packet?.time ?? null;
  }

  const packet = await prismaTelemtry.packet.findUnique({
    where: { packet_id: packetId },
    select: { time: true },
  });
  return packet?.time ?? null;
}

export async function findReplayPacketAtOrAfter(
  car: SupportedCar | null | undefined,
  packetStart: PacketScalar,
  packetEnd: PacketScalar,
  atTimeMs: PacketScalar,
): Promise<ReplayPacket | null> {
  if (car === "angelique") {
    const packet = await prismaAngelique.packet.findFirst({
      where: {
        packet_id: { gte: packetStart, lte: packetEnd },
        time: { gte: atTimeMs },
      },
      orderBy: { packet_id: "asc" },
      include: { dynamics: true, controls: true },
    });
    if (!packet) return null;
    return {
      packet_id: packet.packet_id,
      time: packet.time,
      dynamics: packet.dynamics as unknown as Record<string, unknown> | null,
      controls: packet.controls as unknown as Record<string, unknown> | null,
    };
  }

  const packet = await prismaTelemtry.packet.findFirst({
    where: {
      packet_id: { gte: packetStart, lte: packetEnd },
      time: { gte: atTimeMs },
    },
    orderBy: { packet_id: "asc" },
    include: { dynamics: true, controls: true },
  });
  if (!packet) return null;
  return {
    packet_id: packet.packet_id,
    time: packet.time,
    dynamics: packet.dynamics as unknown as Record<string, unknown> | null,
    controls: packet.controls as unknown as Record<string, unknown> | null,
  };
}

export async function findNextPacketIdInRange(
  car: SupportedCar | null | undefined,
  packetId: PacketScalar,
  packetEnd: PacketScalar,
): Promise<PacketScalar | null> {
  if (car === "angelique") {
    const nextPacket = await prismaAngelique.packet.findFirst({
      where: {
        packet_id: { gt: packetId, lte: packetEnd },
      },
      orderBy: { packet_id: "asc" },
      select: { packet_id: true },
    });
    return nextPacket?.packet_id ?? null;
  }

  const nextPacket = await prismaTelemtry.packet.findFirst({
    where: {
      packet_id: { gt: packetId, lte: packetEnd },
    },
    orderBy: { packet_id: "asc" },
    select: { packet_id: true },
  });
  return nextPacket?.packet_id ?? null;
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
  const driveDay = await prismaTelemtry.drive_day.findUnique({
    where: { day_id: eventId },
    select: {
      car_id: true,
      car: { select: { car_name: true } },
    },
  });
  if (!driveDay) return null;
  return normalizeCar(driveDay.car?.car_name) ?? (await resolveCarFromCarId(driveDay.car_id));
}
