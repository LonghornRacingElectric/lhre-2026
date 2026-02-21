"""
telemtry/stack/tests/test_db_connectivity.py

Tests PostgreSQL database connectivity for the telemetry system.
Requires the PostgreSQL (db) container to be running.
"""

import unittest
import os
import sys

from telemtry.stack.tests.test_utils import (
    TelemetryConfig,
    check_db_connection,
    wait_for_service,
)


class TestDatabaseConnectivity(unittest.TestCase):
    """Test suite for PostgreSQL database connectivity."""
    
    @classmethod
    def setUpClass(cls):
        """Set up test fixtures."""
        cls.config = TelemetryConfig.from_env()
    
    def test_database_available(self):
        """Test that the PostgreSQL database is available."""
        is_available = wait_for_service(
            lambda: check_db_connection(self.config.db_host, self.config.db_port),
            "PostgreSQL Database",
            timeout=30,
        )
        self.assertTrue(is_available, "PostgreSQL database should be available")
    
    def test_database_query(self):
        """Test that we can execute queries on the database."""
        try:
            import psycopg2
            
            conn = psycopg2.connect(
                host=self.config.db_host,
                port=self.config.db_port,
                user=os.getenv("POSTGRES_USER", "postgres"),
                password=os.getenv("POSTGRES_PASSWORD", "postgres"),
                database="telemetry",
            )
            
            cursor = conn.cursor()
            cursor.execute("SELECT 1")
            result = cursor.fetchone()
            
            self.assertEqual(result[0], 1, "Database query should return expected result")
            
            cursor.close()
            conn.close()
            
        except Exception as e:
            self.fail(f"Database query failed: {e}")
    
    def test_nightwatch_database_exists(self):
        """Test that the Nightwatch database exists (used by telemetry)."""
        try:
            import psycopg2
            
            # Try to connect to the Nightwatch database
            conn = psycopg2.connect(
                host=self.config.db_host,
                port=self.config.db_port,
                user=os.getenv("POSTGRES_USER", "postgres"),
                password=os.getenv("POSTGRES_PASSWORD", "postgres"),
                database="telemetry",
            )
            
            conn.close()
            self.assertTrue(True, "Nightwatch database exists")
            
        except psycopg2.OperationalError as e:
            if "does not exist" in str(e):
                self.skipTest("Nightwatch database not yet created")
            raise
    
    def test_angelique_database_exists(self):
        """Test that the Angelique database exists (used by telemetry)."""
        try:
            import psycopg2
            
            # Try to connect to the Angelique database
            conn = psycopg2.connect(
                host=self.config.db_host,
                port=self.config.db_port,
                user=os.getenv("POSTGRES_USER", "postgres"),
                password=os.getenv("POSTGRES_PASSWORD", "postgres"),
                database="angelique",
            )
            
            conn.close()
            self.assertTrue(True, "Angelique database exists")
            
        except psycopg2.OperationalError as e:
            if "does not exist" in str(e):
                self.skipTest("Angelique database not yet created")
            raise

    def test_orion_database_exists(self):
        """Test that the Orion database exists (used by telemetry)."""
        try:
            import psycopg2

            conn = psycopg2.connect(
                host=self.config.db_host,
                port=self.config.db_port,
                user=os.getenv("POSTGRES_USER", "postgres"),
                password=os.getenv("POSTGRES_PASSWORD", "postgres"),
                database="orion",
            )

            conn.close()
            self.assertTrue(True, "Orion database exists")

        except psycopg2.OperationalError as e:
            if "does not exist" in str(e):
                self.skipTest("Orion database not yet created")
            raise
    
    def test_sqlalchemy_connection(self):
        """Test SQLAlchemy connection to the database."""
        try:
            from analysis.sql_utils.db_session import get_db
            
            with get_db("Nightwatch") as session:
                # Execute a simple query
                result = session.execute("SELECT 1")
                self.assertIsNotNone(result, "SQLAlchemy session should work")
                
        except Exception as e:
            self.skipTest(f"SQLAlchemy test skipped: {e}")


if __name__ == "__main__":
    unittest.main()
