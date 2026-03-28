import datetime

from analysis.sql_utils.models import Partitions


class PartitionManager:
    def __init__(self, gap_seconds: int = 300):
        self.partition_gap_ms = int(gap_seconds) * 1000

    def record_packet_time(self, session, packet_time_ms: int) -> None:
        latest = session.query(Partitions).order_by(Partitions.end_time.desc()).first()

        if latest is None:
            session.add(
                Partitions(
                    partition_name=datetime.datetime.fromtimestamp(packet_time_ms / 1000).strftime('%Y-%m-%d %H:%M:%S'),
                    start_time=packet_time_ms,
                    end_time=packet_time_ms,
                )
            )
            return

        if packet_time_ms - int(latest.end_time) > self.partition_gap_ms:
            session.add(
                Partitions(
                    partition_name=datetime.datetime.fromtimestamp(packet_time_ms / 1000).strftime('%Y-%m-%d %H:%M:%S'),
                    start_time=packet_time_ms,
                    end_time=packet_time_ms,
                )
            )
            return

        if packet_time_ms > int(latest.end_time):
            latest.end_time = packet_time_ms

    def set_latest_end_time(self, session, packet_time_ms: int) -> None:
        latest = session.query(Partitions).order_by(Partitions.end_time.desc()).first()
        if latest is not None and packet_time_ms > int(latest.end_time):
            latest.end_time = packet_time_ms
