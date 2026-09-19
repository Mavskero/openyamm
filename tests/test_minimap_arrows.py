"""Headless world binding, cache invalidation and malformed metadata coverage."""
import os
from pathlib import Path
import subprocess
import zipfile

import pytest

TOOL = Path(os.environ.get('MINIMAP_ARROW_CHECK_TOOL', Path(__file__).parents[1] / 'build-apple/minimap_arrow_check'))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason='Build minimap_arrow_check first')


def package(root, mode, fault=None):
    (root / 'worlds').mkdir(parents=True)
    with zipfile.ZipFile(root / 'engine.zip', 'w') as archive:
        archive.writestr('placeholder', b'')
        # Deliberately competing engine images must not replace missing world-owned art.
        for world in ('mm6', 'mm7'):
            for direction in range(1, 9):
                archive.writestr(f'icons/{world[-1]}mapdir{direction}.bmp', b'competing image')
    for world in ('mm6', 'mm7'):
        rows = ['Direction\tTexture'] + [f'{i}\t{world[-1]}mapdir{i}.bmp' for i in range(1, 9)]
        if fault == 'header':
            rows[0] = 'Direction\tOther'
        elif fault == 'order':
            rows[1], rows[2] = rows[2], rows[1]
        elif fault == 'traversal':
            rows[1] = '1\t../mapdir1.bmp'
        elif fault == 'missing_row':
            rows.pop()
        elif fault == 'extra_row':
            rows.append('9\tmapdir9.bmp')
        elif fault == 'extra_column':
            rows[1] += '\tignored'
        elif fault == 'empty_stem':
            rows[1] = '1\t.bmp'
        elif fault == 'oversized':
            rows[1] = '1\t' + 'a' * 4100 + '.bmp'
        with zipfile.ZipFile(root / f'worlds/{world}.zip', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('placeholder', b'')
            archive.writestr('namespace.bin', bytes((i * 37 + int(world[-1])) % 256 for i in range(65537)))
            if mode != 'legacy':
                archive.writestr('ui/minimap_arrows.txt', '\n'.join(rows) + '\n')
            if mode == 'world':
                for direction in range(1, 9):
                    archive.writestr(f'icons/{world[-1]}mapdir{direction}.bmp', b'world image')


@pytest.mark.parametrize('mode', ['world', 'legacy', 'missing'])
def test_world_bindings_and_legacy_package_convention(tmp_path, mode):
    package(tmp_path, mode)
    result = subprocess.run([str(TOOL), str(tmp_path), mode], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('fault', ['header', 'order', 'traversal', 'missing_row', 'extra_row',
                                   'extra_column', 'empty_stem', 'oversized'])
def test_invalid_declared_binding_does_not_fall_back(tmp_path, fault):
    package(tmp_path, 'rejected', fault)
    result = subprocess.run([str(TOOL), str(tmp_path), 'rejected'], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
