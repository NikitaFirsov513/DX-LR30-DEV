"""Download only the explicitly listed public vendor documents; keep a provenance index."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def fetch(entry, refresh):
    path = ROOT/'docs'/entry['file']
    item = dict(entry)
    try:
        if (refresh or not path.exists()) and not entry.get('local_only'):
            request = urllib.request.Request(entry['url'], headers={'User-Agent':'Mozilla/5.0'})
            if os.name == 'nt' and 'www.stmcu.jp/' in entry['url']:
                # Schannel resolves the regional server's intermediate chain using Windows trust.
                result = subprocess.run(['curl.exe','-fsSL','--max-time','180',entry['url']],
                                        capture_output=True, check=True)
                data = result.stdout
                item['resolved_url'] = entry['url']
            else:
                with urllib.request.urlopen(request, timeout=30) as response:
                    data = response.read()
                    item['resolved_url'] = response.url
            if not data.startswith(b'%PDF-'):
                raise ValueError('Server returned a non-PDF response')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        data = path.read_bytes()
        item.update(status='ok', bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        print('OK', entry['file'], len(data), flush=True)
    except Exception as error:
        item.update(status='unavailable', error=str(error))
        print('UNAVAILABLE', entry['file'], str(error), flush=True)
    return item


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh', action='store_true', help='Explicitly update the saved document snapshots')
    args = parser.parse_args()
    entries = json.loads((ROOT/'docs/sources.json').read_text())
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda item: fetch(item, args.refresh), entries))
    manifest = ROOT/'docs/download-manifest.json'
    manifest.write_text(json.dumps({'checked_at':datetime.now(timezone.utc).isoformat(), 'documents':results}, indent=2)+'\n')
    if any(item['status'] != 'ok' for item in results):
        raise SystemExit('Some documents were unavailable; see docs/download-manifest.json. Existing snapshots were preserved.')


if __name__ == '__main__':
    main()
