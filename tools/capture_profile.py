"""Capture the same editor workflow twice using the matching Tracy CLI."""
import argparse
import os
from pathlib import Path
import subprocess
import time

parser=argparse.ArgumentParser()
parser.add_argument('--editor',type=Path,required=True)
parser.add_argument('--capture',type=Path,required=True)
parser.add_argument('--output',type=Path,default=Path('docs/evidence'))
args=parser.parse_args(); args.output.mkdir(parents=True,exist_ok=True)
for mode,enabled in [('main','0'),('worker','1')]:
    trace=(args.output/f'windows-{mode}.tracy').resolve()
    csv=(args.output/f'windows-{mode}-frames.csv').resolve()
    env=os.environ.copy(); env.update(GD_PHYSICS_JOBS=enabled,GD_FRAME_CSV=str(csv))
    env.pop('GD_FRAME_LIMIT',None)
    with (args.output/f'tracy-{mode}.txt').open('w') as log:
        capture=subprocess.Popen([str(args.capture.resolve()),'-a','127.0.0.1','-o',str(trace),'-f'],
            stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        try:
            time.sleep(.25)
            result=subprocess.run([str(args.editor.resolve()),'--demo'],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
            (args.output/f'editor-{mode}-verification.txt').write_text(result.stdout)
            if result.returncode or 'DEMO WORKFLOW PASSED' not in result.stdout:
                raise RuntimeError('Editor verification failed: '+result.stdout[-2000:])
            if capture.wait(timeout=30): raise RuntimeError('Tracy capture failed; inspect '+log.name)
            if not trace.is_file() or trace.stat().st_size==0: raise RuntimeError('Missing Tracy capture')
            print(f'{mode}: verified workflow; {trace.stat().st_size:,} bytes captured')
        finally:
            if capture.poll() is None:
                capture.terminate(); capture.wait(timeout=10)
