"""Generate EIDE's source/target configuration from project.json (not used by CLI builds)."""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    manifest = json.loads((ROOT/'project.json').read_text())
    # EIDE keeps its own build options; CMake is the canonical terminal build.
    options = {
        'version': 5, 'afterBuildTasks': [], 'beforeBuildTasks': [],
        'asm-compiler': {'ASM_FLAGS': ''},
        'c/cpp-compiler': {'CXX_FLAGS': '', 'C_FLAGS': '-Wextra -Werror -fno-common -Wno-unused-parameter',
            'language-c': 'c11', 'language-cpp': 'c++11', 'one-elf-section-per-data': True,
            'one-elf-section-per-function': True, 'optimization': 'level-size', 'warnings': 'all-warnings'},
        'global': {'$float-abi-type': 'soft', 'misc-control': [], 'not-use-syscalls': True,
            'output-debug-info': 'enable', 'use-newlib-nano': True},
        'linker': {'$outputTaskExcludes': [], 'LD_FLAGS': '', 'LIB_FLAGS': '',
            'output-format': 'elf', 'remove-unused-input-sections': True}
    }
    config = {'version':'4.1', 'name':manifest['name'], 'type':'ARM', 'deviceName':'STM32F103C8',
        'packDir':None, 'srcDirs':manifest['source_dirs'],
        'virtualFolder':{'name':'<virtual_root>', 'files':[], 'folders':[]},
        'dependenceList':[], 'outDir':'build/eide', 'miscInfo':{}, 'targets':{}}
    for name, defines in manifest['targets'].items():
        config['targets'][name] = {
            'cppPreprocessAttrs':{'defineList':manifest['defines']+defines,
                'incList':manifest['include_dirs'], 'libList':[]},
            'excludeList':[], 'settings':{}, 'toolchain':'GCC',
            'toolchainConfigMap':{'GCC':{'archExtensions':'', 'cpuType':'Cortex-M3',
                'floatingPointHardware':'none', 'options':copy.deepcopy(options),
                'scatterFilePath':manifest['linker_script'], 'storageLayout':{'RAM':[], 'ROM':[]},
                'useCustomScatterFile':True}},
            'uploadConfigMap':{'Custom':{'bin':'', 'commandLine':
                'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "tools/flash.ps1" -Firmware "${programFile}"',
                'eraseChipCommand':''}}, 'uploader':'Custom'
        }
    (ROOT/'.eide/eide.yml').write_text(json.dumps(config, indent=2)+'\n', encoding='utf-8')
    print('EIDE configuration generated from project.json. Reopen the PJ26 workspace; close it BEFORE future regeneration to avoid EIDE overwriting changes from its cache.')


if __name__ == '__main__':
    main()
