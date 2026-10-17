"""Build the public release ZIP: a seplugins/viz_tape folder to copy onto the
memory card, plus the docs. Never includes firmware or Sony resources."""
import hashlib
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

root = Path(__file__).resolve().parents[1]
version = '1.3'
card = 'seplugins/viz_tape/'
files = {
    card + 'viz_tape.prx': 'viz_tape.prx',
    'INSTALL.txt': 'INSTALL.txt',
    'README.md': 'README.md',
    'CHANGELOG.md': 'CHANGELOG.md',
    'LICENSE': 'LICENSE',
}
contents = {name: (root/source).read_bytes() for name, source in files.items()}
contents['SHA256SUMS'] = ''.join(
    f'{hashlib.sha256(data).hexdigest()}  {name}\n'
    for name, data in contents.items()).encode('ascii')
out = root/'releases'
out.mkdir(exist_ok=True)
archive = out/f'viz_tape-v{version}.zip'
with ZipFile(archive, 'w', compression=ZIP_DEFLATED) as package:
    for name, data in contents.items():
        info = ZipInfo(name, date_time=(2026, 10, 15, 0, 0, 0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        package.writestr(info, data)
with ZipFile(archive) as package:
    assert package.testzip() is None
    assert set(package.namelist()) == set(contents)
    assert [name for name in package.namelist() if name.endswith('.prx')] == [card + 'viz_tape.prx']
    assert not any(name.endswith(('.rco', '.gim', '.bin', '.elf', '.o')) for name in package.namelist())
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n')
print(f'Release ready: {archive.relative_to(root)} ({archive.stat().st_size:,} bytes)')
