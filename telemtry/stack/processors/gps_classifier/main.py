import logging
from gps_classifier import run_processor

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    run_processor()