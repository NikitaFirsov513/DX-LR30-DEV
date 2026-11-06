"""Build both firmware variants, run linked-image checks and native C tests; never flash."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
from build import ROOT, find_tool


def run(command):
    subprocess.run([str(part) for part in command], cwd=ROOT, check=True)


def main():
    for target in ['Radio','UART']:
        run([sys.executable, ROOT/'tools/build.py', '--target', target])
    run([sys.executable, ROOT/'tools/verify.py'])
    cmake = find_tool('cmake', 'CMAKE_EXE', [Path('C:/Program Files/CMake/bin/cmake.exe')])
    out = ROOT/'build/host'
    command = [cmake,'-S',ROOT/'tests','-B',out]
    if not (out/'CMakeCache.txt').exists() and os.name == 'nt' and not shutil.which('cc'):
        command += ['-G','Visual Studio 17 2022','-A','x64']
    run(command)
    run([cmake,'--build',out,'--config','Debug','--parallel'])
    ctest = Path(cmake).with_name('ctest.exe' if os.name == 'nt' else 'ctest')
    run([ctest,'--test-dir',out,'-C','Debug','--output-on-failure'])
    if os.name == 'nt':
        for test in ['test-flasher.ps1','test-serial-ports.ps1','test-link-host.ps1']:
            run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',ROOT/'tools'/test])
    print('PASS: firmware builds, linked images, board map, native C tests and host flasher tests. Hardware tests are separate.')


if __name__ == '__main__':
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
