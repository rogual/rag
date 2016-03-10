#!/usr/bin/env python3

import subprocess
import tempfile
import sys
import os

query = sys.argv[1]
replacement = sys.argv[2]
dir = sys.argv[3] if len(sys.argv) > 3 else '.'

results = \
    subprocess.check_output(['ag', '-l', query, dir]).strip().split(b'\n')


sed_result = tempfile.NamedTemporaryFile(delete=False)
sed_result.close()

for result in results:

    filename = result.decode('utf-8')

    r = subprocess.call(
        '''sed 's/%s/%s/g' < "%s" > "%s"''' % (query, replacement, filename, sed_result.name),
        shell=True
    )

    if r != 0:
        print('Sed failed on %s' % filename)
        continue

    print('\nMatches in %s:\n\n' % filename)

    subprocess.call(
        '''git --no-pager diff --word-diff "%s" "%s"''' % (filename, sed_result.name),
        shell=True
    )

    cmd = input("Write changes to %s ? [y/n] " % filename)

    if cmd == 'y':
        subprocess.check_call(
            '''mv %s %s''' % (sed_result.name, filename),
            shell=True
        )
    else:
        os.unlink(sed_result.name)
