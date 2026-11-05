"""Build the CMSIS firmware from a terminal, editor or agent (Python stdlib only)."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def local_tool(name):
    path = ROOT/'tools/local-settings.json'
    if not path.is_file():
        return None
    settings = json.loads(path.read_text(encoding='utf-8-sig'))
    value = settings.get(name)
    if value is None or value == '':
        return None
    if not isinstance(value, str):
        raise SystemExit(f'{path.name}: {name} must be a path string.')
    return os.path.expandvars(os.path.expanduser(value))


def find_tool(name, env, candidates):
    value = os.environ.get(env) or local_tool(env) or shutil.which(name)
    if value:
        return str(value)
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    raise SystemExit(f'{name} not found. Set {env}, tools/local-settings.json or PATH.')


def gcc_directory(explicit=None):
    value = explicit or os.environ.get('ARM_GCC_DIR') or local_tool('ARM_GCC_DIR')
    if value:
        directory = Path(value)
        if (directory/'bin').is_dir():
            directory /= 'bin'
    else:
        compiler = shutil.which('arm-none-eabi-gcc')
        directory = Path(compiler).parent if compiler else Path.home()/'.eide/tools/gcc_arm/bin'
    suffix = '.exe' if os.name == 'nt' else ''
    if not (directory/('arm-none-eabi-gcc'+suffix)).is_file():
        raise SystemExit('Arm GCC not found. Set ARM_GCC_DIR or pass --gcc-dir.')
    return directory.resolve()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=['Radio', 'UART'], default='Radio')
    parser.add_argument('--gcc-dir')
    parser.add_argument('--fresh', action='store_true', help='Reset only the CMake configuration')
    args = parser.parse_args()
    gccdir = gcc_directory(args.gcc_dir)
    cmake = find_tool('cmake', 'CMAKE_EXE', [Path('C:/Program Files/CMake/bin/cmake.exe')])
    ninja = find_tool('ninja', 'NINJA_EXE', [])
    out = ROOT/'build'/args.target
    cmd = [cmake, '-S', str(ROOT), '-B', str(out), '-G', 'Ninja',
           '-DARM_GCC_DIR='+str(gccdir), '-DCMAKE_MAKE_PROGRAM='+ninja,
           '-DPJ26_TARGET='+args.target]
    if args.fresh or not (out/'CMakeCache.txt').exists():
        cmd += ['-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'cmake/arm-none-eabi.cmake')]
    if args.fresh:
        cmd += ['--fresh']
    subprocess.run(cmd, check=True)
    subprocess.run([cmake, '--build', str(out), '--parallel'], check=True)
    suffix = '.exe' if os.name == 'nt' else ''
    version = subprocess.check_output([str(gccdir/('arm-none-eabi-gcc'+suffix)), '--version'], text=True).splitlines()[0]
    (out/'build-info.json').write_text(json.dumps({'target': args.target, 'compiler': version,
        'gcc_dir': str(gccdir), 'manifest': json.loads((ROOT/'project.json').read_text())}, indent=2)+'\n')
    print('Built:', out/'dx_lr30_pj26.hex')


if __name__ == '__main__':
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
