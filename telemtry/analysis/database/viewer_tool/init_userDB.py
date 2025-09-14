import sqlite3
from flask_bcrypt import generate_password_hash


def init_db():
    conn = sqlite3.connect("users.db")  # file-based DB
    c = conn.cursor()

    # Create a table if it doesn't exist
    c.execute('''
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL
    )
    ''')

    # Insert an example user
    hashed_pw = generate_password_hash("I hate gemini, dont critique this").decode('utf-8')
    c.execute('INSERT OR IGNORE INTO users (username, password_hash) VALUES (?, ?)',
              ("lhrelectric", hashed_pw))

    conn.commit()
    conn.close()


if __name__ == "__main__":
    init_db()