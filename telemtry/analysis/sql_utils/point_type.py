from sqlalchemy.types import UserDefinedType

class PointType(UserDefinedType):
    """
    Custom SQLAlchemy type to handle PostgreSQL POINT columns.
    """

    cache_ok = True

    def get_col_spec(self):
        return "POINT"

    def bind_processor(self, dialect):
        def process(value):
            if value is None:
                return None
            # Allow already-formatted string values
            if isinstance(value, str):
                return value
            if isinstance(value, tuple) and len(value) == 2:
                # PostgreSQL POINT format is '(x,y)'
                return f"({value[0]},{value[1]})"
            if isinstance(value, list) and len(value) == 2:
                return f"({value[0]},{value[1]})"
            raise TypeError(f"Expected a tuple of (x, y) or str, got {type(value)}")
        return process

    def result_processor(self, dialect, coltype):
        def process(value):
            if value is None:
                return None
            if isinstance(value, tuple) and len(value) == 2:
                return (float(value[0]), float(value[1]))
            if isinstance(value, str):
                if value.startswith("(") and value.endswith(")"):
                    value = value[1:-1]
                x_str, y_str = value.split(",")
                return (float(x_str), float(y_str))
            if isinstance(value, list) and len(value) == 2:
                return (float(value[0]), float(value[1]))
            raise ValueError(f"Unexpected POINT format: {value!r}")
        return process