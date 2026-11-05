"""Check linked HEX images and CubeMX/exported pin assignments without hardware."""
import csv
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TOOL = Path.home()/'.eide/tools/gcc_arm/bin/arm-none-eabi-nm.exe'

def read_hex(path):
    memory, base = {}, 0
    for line in path.read_text().splitlines():
        assert line.startswith(':'), 'Invalid Intel HEX record'
        record = bytes.fromhex(line[1:])
        assert sum(record) & 255 == 0, 'Bad Intel HEX checksum'
        count, offset, kind = record[0], int.from_bytes(record[1:3], 'big'), record[3]
        data = record[4:4+count]
        assert len(record) == count+5
        if kind == 4:
            base = int.from_bytes(data, 'big') << 16
        elif kind == 0:
            for index, value in enumerate(data):
                memory[base+offset+index] = value
    return memory

def verify_image(target):
    out = ROOT/'build'/target
    memory = read_hex(out/'dx_lr30_pj26.hex')
    assert min(memory) == 0x08000000 and max(memory) < 0x08010000
    def word(address):
        return struct.unpack('<I', bytes(memory[address+i] for i in range(4)))[0]
    assert word(0x08000000) == 0x20005000, 'Incorrect/unaligned C8 initial stack'
    assert 0x08000001 <= word(0x08000004) < 0x08010000 and word(0x08000004) & 1
    text = subprocess.check_output([str(TOOL), '-n', str(out/'dx_lr30_pj26.elf')], text=True)
    symbols = {}
    for line in text.splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    for irq, name in [(37, 'USART1_IRQHandler'), (29, 'TIM3_IRQHandler')]:
        assert word(0x08000000 + 4*(16+irq)) == symbols[name] | 1, name
    if target == 'Radio':
        assert word(0x08000000+4*(16+40)) == symbols['EXTI15_10_IRQHandler'] | 1
    assert symbols['_end'] + 0x200 + 0x800 <= 0x20005000, 'RAM/stack overflow'
    print(target, 'PASS: HEX checksums, C8 flash/stack/RAM, linked IRQ vectors')

def verify_pinout():
    with (ROOT/'cubemx/pinout.csv').open(encoding='utf-8-sig', newline='') as handle:
        pins = {r['Name']:r for r in csv.DictReader(handle)}
    expected = {'PA0-WKUP':'RADIO_TXEN','PA1':'RADIO_RXEN','PA2':'RADIO_BUSY',
                'PA3':'RADIO_NRST','PA4':'RADIO_NSS','PC15-OSC32_OUT':'RADIO_DIO1'}
    for pin, label in expected.items():
        assert pins[pin]['Label'] == label
    for pin, signal in [('PA5','SPI1_SCK'), ('PA6','SPI1_MISO'), ('PA7','SPI1_MOSI'),
                        ('PA9','USART1_TX'), ('PA10','USART1_RX')]:
        assert pins[pin]['Signal'] == signal
    assert all('DIO2' not in p['Label'] for p in pins.values())
    json.loads((ROOT/'.eide/eide.yml').read_text(encoding='utf8'))
    print('CubeMX-exported pinout / EIDE configuration PASS')

if __name__ == '__main__':
    verify_pinout()
    for target in ['Radio', 'UART']:
        verify_image(target)
