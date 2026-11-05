"""Explicit fallback downloads of original ST PDFs from archives when st.com is unreachable."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import json
from fetch_docs import ROOT, fetch


def main():
    entries = json.loads((ROOT/'docs/fallback-sources.json').read_text())
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda item: fetch(item, False), entries))
    (ROOT/'docs/archive-manifest.json').write_text(json.dumps({
        'checked_at':datetime.now(timezone.utc).isoformat(), 'documents':results},indent=2)+'\n')
    if any(item['status'] != 'ok' for item in results):
        raise SystemExit('Some archived documents unavailable; see docs/archive-manifest.json')


if __name__ == '__main__':
    main()
