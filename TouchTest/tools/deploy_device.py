#!/usr/bin/env python3
"""Deploy through the public SDK tools (also usable with WSL USB serial)."""
from pathlib import Path
import argparse
import sys
p = Path(__file__).resolve().parents[1]
sdk = p.parent.parent / 'aster-sdk'
sys.path.insert(0, str(sdk / 'tools'))
import aster_send, aster_board, deploy
parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
args = parser.parse_args()
def shell(command, timeout=30):
    response = deploy.shell_command(args.port, command, timeout)
    print(response, flush=True)
    return response
info = shell('aster sys info')
if 'arch=xtensa' not in info or 'abi=4.1.' not in info:
    raise SystemExit('Device target or ABI does not match this package')
package = p / 'dist/xtensa/aster.touchtest.ast'
result = aster_send.send_file(args.port, package, '/SD:/aster.touchtest.ast', target=aster_board.get('xtensa'))
if result != 'OK':
    raise SystemExit('Upload failed: ' + str(result))
# Never stop another APP automatically. Installation reports EBUSY if necessary.
response = shell('aster app install /SD:/aster.touchtest.ast')
if 'installed OK' not in response:
    raise SystemExit('Install failed; see response above')
response = shell('aster app start aster.touchtest')
if 'started aster.touchtest' not in response:
    raise SystemExit('Start failed; see response above')
shell('aster app state aster.touchtest')