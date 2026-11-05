"""Validate the actual linked firmware, pin map and CubeMX reference (no hardware access)."""
import argparse
import csv
import json
import hashlib
from pathlib import Path
import re
import struct
import subprocess
from build import gcc_directory

ROOT = Path(__file__).resolve().parents[1]


def verify_vendor():
    lock = json.loads((ROOT/'vendor/lock.json').read_text())['files']
    for name, expected in lock.items():
        assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, f'Vendor snapshot changed: {name}'
    for path in (ROOT/'vendor').rglob('*'):
        if path.is_file() and path.suffix.lower() in {'.c','.h','.s'}:
            assert path.relative_to(ROOT).as_posix() in lock, f'Untracked vendor source: {path.name}'
    print('Vendor: PASS - pinned CMSIS/ST/Semtech source checksums')


def read_hex(path):
    memory, base, ended = {}, 0, False
    for line in path.read_text().splitlines():
        assert not ended and line.startswith(':'), 'Invalid Intel HEX record/order'
        record = bytes.fromhex(line[1:])
        assert len(record) >= 5 and len(record) == record[0]+5 and sum(record) & 255 == 0, 'HEX checksum/length'
        count, offset, kind = record[0], int.from_bytes(record[1:3], 'big'), record[3]
        data = record[4:4+count]
        if kind == 4:
            assert count == 2
            base = int.from_bytes(data, 'big') << 16
        elif kind == 0:
            for index, value in enumerate(data):
                address = base + offset + index
                assert address not in memory, 'Overlapping HEX records'
                memory[address] = value
        elif kind == 1:
            assert count == 0
            ended = True
        else:
            assert kind == 5, 'Unsupported HEX record type'
    assert ended, 'HEX EOF missing'
    return memory


def verify_image(target, base):
    manifest = json.loads((ROOT/'project.json').read_text())
    out = base/target
    memory = read_hex(out/'dx_lr30_pj26.hex')
    assert min(memory) == 0x08000000 and max(memory) < 0x08000000 + manifest['flash_bytes']
    def word(address):
        return struct.unpack('<I', bytes(memory[address+i] for i in range(4)))[0]
    assert word(0x08000000) == 0x20000000 + manifest['ram_bytes'], 'C8 initial stack'
    assert 0x08000001 <= word(0x08000004) < 0x08010000 and word(0x08000004) & 1
    gcc = gcc_directory()
    tool = gcc/('arm-none-eabi-nm.exe' if (gcc/'arm-none-eabi-nm.exe').exists() else 'arm-none-eabi-nm')
    listing = subprocess.check_output([str(tool), '-n', str(out/'dx_lr30_pj26.elf')], text=True)
    symbols = {}
    for line in listing.splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    for vector, name in [(1,'Reset_Handler'),(3,'HardFault_Handler'),(15,'SysTick_Handler'),
                         (16+37,'USART1_IRQHandler'),(16+29,'TIM3_IRQHandler')]:
        assert word(0x08000000 + 4*vector) == symbols[name] | 1, name
    if target == 'Radio':
        assert word(0x08000000+4*(16+40)) == symbols['EXTI15_10_IRQHandler'] | 1
    assert symbols['_end'] + 0x200 + 0x800 <= 0x20005000, 'RAM/stack overflow'
    assert 'HAL_Init' not in symbols and 'HAL_UART_Init' not in symbols, 'STM32 HAL unexpectedly linked'
    binary = (out/'dx_lr30_pj26.bin').read_bytes()
    assert len(binary) == max(memory)-0x08000000+1
    assert all(binary[address-0x08000000] == value for address,value in memory.items()), 'HEX/BIN mismatch'
    print(f'{target}: PASS - HEX/BIN, 64K Flash, 20K RAM, reset/IRQ vectors, no STM32 HAL')


def verify_pinout():
    rows = re.findall(r'^BOARD_PIN\((\w+),\s*([A-C]),\s*(\d+),\s*(\w+),\s*([01])\)',
                      (ROOT/'board/pins.def').read_text(), re.M)
    assert len(rows) == 13 and len({(port,bit) for _,port,bit,_,_ in rows}) == len(rows), 'Duplicate/malformed board pins'
    ioc = dict(line.split('=',1) for line in (ROOT/'cubemx/dx_lr30_pj26.ioc').read_text().splitlines()
               if '=' in line and not line.startswith('#'))
    with (ROOT/'cubemx/pinout.csv').open(encoding='utf-8-sig', newline='') as handle:
        exported = {re.match(r'P[A-Z]\d+', row['Name'])[0]:row for row in csv.DictReader(handle)
                    if re.match(r'P[A-Z]\d+', row['Name'])}
    for name,port,bit,mode,level in rows:
        pin = f'P{port}{int(bit)}'
        assert pin not in {'PA13','PA14','PD0','PD1'}, 'Reserved SWD/HSE pin reused'
        keys = [key for key in ioc if re.match(re.escape(pin)+r'(?:[.-])', key)]
        assert keys, f'{pin} missing from CubeMX'
        values = {key.rsplit('.',1)[-1]:ioc[key] for key in keys}
        if name.startswith(('SPI1_','USART1_')):
            assert values.get('Signal') == name and exported[pin]['Signal'] == name, name
        else:
            assert values.get('GPIO_Label') == name and exported[pin]['Label'] == name, name
        if 'OUTPUT' in mode:
            assert values.get('Signal') == 'GPIO_Output', name
            actual = values.get('PinState','GPIO_PIN_RESET')
            assert actual == ('GPIO_PIN_SET' if level == '1' else 'GPIO_PIN_RESET'), f'{name} initial level'
    assert 'RADIO_DIO2' not in {row[0] for row in rows}
    for key,value in [('RCC.HSE_VALUE','8000000'),('RCC.SYSCLKFreq_VALUE','72000000'),
                      ('RCC.APB1Freq_Value','36000000'),('USART1.BaudRate','9600'),
                      ('SPI1.BaudRatePrescaler','SPI_BAUDRATEPRESCALER_16')]:
        assert ioc.get(key) == value, key
    print('Board: PASS - unique pins, CubeMX/CSV assignments and output levels, clock/UART/SPI reference')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build')
    parser.add_argument('--target', choices=['Radio','UART','all'], default='all')
    args = parser.parse_args()
    verify_vendor()
    verify_pinout()
    for target in (['Radio','UART'] if args.target == 'all' else [args.target]):
        verify_image(target, args.build_dir)
