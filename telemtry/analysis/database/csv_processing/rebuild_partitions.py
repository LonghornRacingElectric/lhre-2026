import argparse
import logging
from pathlib import Path
import sys

sys.path.append(str(Path(__file__).parents[3]))

from analysis.sql_utils.db_session import get_db
from analysis.database.csv_processing.CSV_to_DB import CSVToDB


def main() -> None:
    parser = argparse.ArgumentParser(description="Rebuild telemetry partitions from packet timestamp gaps.")
    parser.add_argument("--car", default="Orion", help="Car database to target (Orion, Angelique, Nightwatch)")
    parser.add_argument("--gap-seconds", type=int, default=300, help="Gap threshold in seconds used to start a new partition")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    with get_db(args.car) as session:
        worker = CSVToDB(db_session=session, car=args.car)
        worker.csv_event_injection(time_threshold=args.gap_seconds)


if __name__ == "__main__":
    main()
