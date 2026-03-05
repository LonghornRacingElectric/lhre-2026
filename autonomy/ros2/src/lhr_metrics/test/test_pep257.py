from ament_pep257.main import main
import pytest


@pytest.mark.linter
def test_pep257():
    rc = main(argv=['.', 'test'])
    assert rc == 0, 'Found code style errors / warnings'
