
import os
import sys
from pathlib import Path
import logging
import traceback

# Add the project root to the python path
sys.path.append(str(Path(__file__).parents[3]))

from analysis.sql_utils.db_handler import DBHandler, DBTarget, get_table_column_specs

def main():
    try:
        with DBHandler(unsafe=True, target=DBTarget.get(car="Nightwatch")) as handler:
            table_specs = get_table_column_specs(force=True, verbose=True, handler=handler, target=DBTarget.get(car='Nightwatch'))
            logging.debug("--- SCHEMA START ---")
            logging.debug(table_specs)
            logging.debug("--- SCHEMA END ---")
    except Exception as e:
        logging.error(f"An error occurred: {e}")
        logging.error(traceback.format_exc())

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    main()
