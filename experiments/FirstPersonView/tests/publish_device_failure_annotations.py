"""Publish at most eight existing CTest assertions as CI annotations.

Diagnostic only: never reruns a test, changes its result, or modifies its log.
The original CTest failure remains the blocking workflow result.
"""
import argparse
from pathlib import Path
import re

MARKER=re.compile(r'(Full GPU scene failed:|GPU probe failed:|GPU contract check \d+ failed|GPU resource cache:|Vertex lease edge:|Allocation failure probe:|ERROR: (?:AddressSanitizer|LeakSanitizer):|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer):|runtime error:)')

def escape(value):
    return value.replace('%','%25').replace('\r','%0D').replace('\n','%0A')

def messages(lines,limit=8):
    found=[];seen=set()
    for line in lines:
        match=MARKER.search(line)
        if match:
            value=line[match.start():].strip()[:400]
            if value.startswith("Allocation failure probe: checked="):continue
            if value not in seen:
                found.append(value);seen.add(value)
                if len(found)>=limit:break
    return found

def publish(path,write=print):
    if not path.is_file():
        write('::notice title=NCMM diagnostic::CTest log unavailable; original failed step remains blocking.')
        return 0
    with path.open(encoding='utf-8',errors='replace') as stream:
        found=messages(stream)
    for value in found:write('::error title=NCMM device test::'+escape(value))
    if not found:write('::notice title=NCMM diagnostic::No supported assertion marker found; original failed step remains blocking.')
    return len(found)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--log',required=True,type=Path);args=parser.parse_args();publish(args.log)

if __name__=='__main__':main()
