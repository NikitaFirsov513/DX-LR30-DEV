"""Standalone GCC build using the sources and settings in the EIDE project."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['Radio', 'UART'], default='Radio')
    parser.add_argument('--gcc-dir', default=os.environ.get('ARM_GCC_DIR'))
    args = parser.parse_args()
    gccdir = Path(args.gcc_dir) if args.gcc_dir else Path.home()/'.eide/tools/gcc_arm/bin'
    if not (gccdir/'arm-none-eabi-gcc.exe').is_file() and (gccdir/'bin').is_dir():
        gccdir /= 'bin'
    gcc = str(gccdir/'arm-none-eabi-gcc.exe')
    cfg = json.loads((ROOT/'.eide/eide.yml').read_text(encoding='utf8'))
    target = cfg['targets'][args.target]
    pp = target['cppPreprocessAttrs']
    out = ROOT/'build'/args.target
    out.mkdir(parents=True, exist_ok=True)
    common = ['-mcpu=cortex-m3', '-mthumb', '-mfloat-abi=soft', '-Os', '-g3',
              '-ffunction-sections', '-fdata-sections', '-fno-common',
              '-Wall', '-Wextra', '-Wno-unused-parameter', '-std=gnu11']
    common += ['-D'+s for s in pp['defineList']]
    common += ['-I'+s for s in pp['incList']]
    sources = sorted(p for d in cfg['srcDirs'] for p in (ROOT/d).rglob('*')
                     if p.suffix in ['.c', '.s'])
    commands, objs = [], []
    for src in sources:
        obj = out/(src.relative_to(ROOT).as_posix().replace('/', '_')+'.o')
        cmd = [gcc, *common, '-c', str(src.relative_to(ROOT)), '-o', str(obj)]
        commands.append(cmd)
        objs.append(str(obj))
    def compile_one(cmd):
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                              encoding='utf8', errors='replace')
        return proc.returncode, proc.stdout + proc.stderr
    with ThreadPoolExecutor(max_workers=6) as pool:
        results = list(pool.map(compile_one, commands))
    log = ''.join(text for _, text in results)
    (out/'compile.log').write_text(log, encoding='utf8')
    if log:
        print(log)
    if any(code for code, _ in results):
        raise SystemExit('Compilation failed; see build/<target>/compile.log')
    elf = out/'dx_lr30_pj26.elf'
    cmd = [gcc, *common[:3], '-specs=nano.specs', '-specs=nosys.specs',
           '-TSTM32F103C8TX_FLASH.ld', '-Wl,--gc-sections',
           '-Wl,-Map='+str(out/'dx_lr30_pj26.map'), *objs, '-o', str(elf)]
    subprocess.run(cmd, cwd=ROOT, check=True)
    for fmt, suffix in [('ihex', 'hex'), ('binary', 'bin')]:
        subprocess.run([str(gccdir/'arm-none-eabi-objcopy.exe'), '-O', fmt,
                        str(elf), str(elf.with_suffix('.'+suffix))], check=True)
    subprocess.run([str(gccdir/'arm-none-eabi-size.exe'), str(elf)], check=True)
    compile_db = [{'directory':str(ROOT), 'file':cmd[-3], 'arguments':cmd} for cmd in commands]
    (out/'compile_commands.json').write_text(json.dumps(compile_db, indent=2), encoding='utf8')
    print('Built', elf.with_suffix('.hex'))

if __name__ == '__main__':
    main()
