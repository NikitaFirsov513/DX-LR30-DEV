"""Extract reference documents and SVD from the official Keil STM32F1 device pack."""
import hashlib
import io
import json
from pathlib import Path
import urllib.request
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://www.keil.com/pack/Keil.STM32F1xx_DFP.2.4.1.pack'
MEMBERS = {
    'Documents/CD00171190.pdf': 'docs/st/RM0008.pdf',
    'Documents/CD00161566.pdf': 'docs/st/DS5319-STM32F103x8-xB.pdf',
    'Documents/dui0552a_cortex_m3_dgug.pdf': 'docs/arm/Cortex-M3-Generic-User-Guide.pdf',
    'SVD/STM32F103xx.svd': 'debug/STM32F103xx.svd',
}


def main():
    request = urllib.request.Request(URL, headers={'User-Agent':'Mozilla/5.0'})
    with urllib.request.urlopen(request, timeout=60) as response:
        data = response.read()
    records = []
    with ZipFile(io.BytesIO(data)) as pack:
        for member, relative in MEMBERS.items():
            content = pack.read(member)
            target = ROOT/relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            records.append({'file': relative, 'pack_member': member,
                'bytes':len(content), 'sha256':hashlib.sha256(content).hexdigest()})
            print('EXTRACTED', relative, len(content), flush=True)
    (ROOT/'docs/pack-manifest.json').write_text(json.dumps({'url':URL,
        'pack_sha256':hashlib.sha256(data).hexdigest(), 'files':records}, indent=2)+'\n')


if __name__ == '__main__':
    main()
