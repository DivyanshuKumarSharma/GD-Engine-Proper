"""Package a relocatable playable build without modifying source assets."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--config', default='Release')
parser.add_argument('--output', type=Path, default=Path('dist'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
suffix = '.exe' if platform.system() == 'Windows' else ''
entries = []
for name in ['CourseMaker', 'GameDevelopmentProject', 'netcode_demo', 'systems_bench', 'systems_tests']:
    candidates = [args.build / args.config / (name+suffix), args.build / (name+suffix), args.build / 'bin' / (name+suffix)]
    binary = next((p for p in candidates if p.is_file()), None)
    if binary is None:
        raise SystemExit(f'Missing executable: {name}')
    entries.append((binary, binary.name))
for p in sorted((root / 'assets').rglob('*')):
    if p.is_file() and not any(part in {'backups', '.git'} for part in p.parts) and p.suffix not in {'.tmp', '.log'}:
        entries.append((p, p.relative_to(root).as_posix()))
for name in ['README.md', 'docs/PROJECT_C.md', 'docs/CREATOR_GUIDE.md', 'docs/BUILD.md', 'docs/FORMATS_AND_DEPENDENCIES.md']:
    p = root / name
    if p.is_file(): entries.append((p,name))
for p in sorted((root/'docs/evidence').rglob('*')):
    if p.is_file() and p.suffix != '.log': entries.append((p,p.relative_to(root).as_posix()))
# Preserve all bundled dependency license notices as separate files.
for p in sorted((root/'engine/thirdparty').rglob('*')):
    if p.is_file() and (p.name.lower().startswith(('license', 'copying')) or p.name.lower() == 'copyright'):
        entries.append((p, 'licenses/' + p.relative_to(root/'engine/thirdparty').as_posix()))
try:
    revision = subprocess.check_output(['git','-c',f'safe.directory={root.as_posix()}','rev-parse','HEAD'],cwd=root,text=True).strip()
    dirty = bool(subprocess.check_output(['git','-c',f'safe.directory={root.as_posix()}','status','--porcelain'],cwd=root,text=True).strip())
except (OSError, subprocess.CalledProcessError): revision,dirty='unknown',True
manifest = {'platform':platform.platform(),'configuration':args.config,'base_commit':revision,'working_tree_modified':dirty,
            'files':{name:hashlib.sha256(p.read_bytes()).hexdigest() for p,name in entries}}
args.output.mkdir(parents=True,exist_ok=True)
archive=args.output/f'GD-Engine-{platform.system()}.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as out:
    for p,name in entries: out.write(p,name)
    out.writestr('build-manifest.json',json.dumps(manifest,indent=2))
    out.writestr('START-HERE.txt','Run CourseMaker for the editor or GameDevelopmentProject for the shooting course.\nAssets must remain beside the executables.\nWindows requires the Microsoft Visual C++ 2015-2022 x64 runtime.\nLinux requires X11/OpenGL 3.3 and standard system libraries; this is not an AppImage.\nSee docs/PROJECT_C.md for systems, networking and profiling demos.\n')
print(f'{archive.resolve()} ({archive.stat().st_size:,} bytes)')
