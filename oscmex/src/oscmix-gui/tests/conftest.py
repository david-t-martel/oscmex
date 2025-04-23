import pytest

@pytest.fixture(scope='session')
def virtualenv():
    import os
    import subprocess
    venv_path = os.path.join(os.path.dirname(__file__), '.venv')
    if not os.path.exists(venv_path):
        subprocess.run(['python', '-m', 'venv', venv_path])
    activate_script = os.path.join(venv_path, 'Scripts', 'activate_this.py')
    exec(open(activate_script).read(), dict(__file__=activate_script))