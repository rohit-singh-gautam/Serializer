# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Recover typed optional/forward/cyclic references after genuine journal-process crashes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

from managed_journal_crash_test import verify_container


def main():
    """Check independent framing and exact typed graphs using a fresh helper process for every operation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--profile', required=True)
    args = parser.parse_args()
    helper = args.helper.resolve(strict=True)
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    commands = []
    checks = 0
    scenarios = 0
    modes = ('appended', 'sidecar')
    append_points = ('before_append', 'after_header', 'after_payload', 'after_commit', 'after_flush')

    def run(path, mode, operation, point='none', record=0, expected=0):
        """Use no shell; a selected storage interruption must exit abruptly with its distinctive code."""
        command = [str(helper), str(path), mode, operation, point, str(record)]
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)
        commands.append({'argv': command, 'returncode': result.returncode,
                         'stdout': result.stdout, 'stderr': result.stderr})
        (directory / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        assert result.returncode == expected, (operation, point, record, result.returncode,
                                               result.stdout, result.stderr)
        return json.loads(result.stdout) if result.stdout.strip() else None

    def inspect(path, mode, phase, sequence, watermark, cursor, dirty):
        """Repeated fresh recovery cannot replay a typed edge, reservation or history operation twice."""
        nonlocal checks
        expected = {'phase': phase, 'root': 19, 'first': 0 if phase == 'retired' else 7,
                    'second': 0 if phase == 'retired' else 93,
                    'new_id': 131 if phase == 'imported' else 0,
                    'watermark': watermark, 'sequence': sequence, 'cursor': cursor, 'dirty': dirty}
        for _ in range(2):
            actual = run(path, mode, 'inspect')
            assert actual == expected, (path.name, expected, actual)
            assert verify_container(path, mode) == sequence, (path.name, 'independent framing sequence')
            checks += 1

    # Keep independent Python framing/cleanup paths below Windows' legacy limit in deep build trees.
    # Reports remain in the requested build directory; crash data belongs to this private OS-temp scope.
    with tempfile.TemporaryDirectory(prefix='reference-crash-') as temporary:
        fixtures = Path(temporary)
        for mode in modes:
            for operation in ('edit', 'delete', 'reserve', 'reject_import', 'import', 'undo', 'redo'):
                for point in append_points:
                    path = fixtures / f'{mode}-{operation}-{point}'
                    run(path, mode, 'create')
                    if operation in ('undo', 'redo'):
                        run(path, mode, 'edit')
                    if operation == 'redo':
                        run(path, mode, 'undo')
                    # Import emits an identity reservation before its complete-candidate publication record.
                    run(path, mode, operation, point, 2 if operation == 'import' else 1, expected=86)
                    committed = point in ('after_commit', 'after_flush')
                    phase, sequence, watermark, cursor, dirty = 'linked', 0, 130, 0, False
                    if operation == 'edit' and committed:
                        phase, sequence, cursor, dirty = 'edited', 1, 1, True
                    elif operation == 'delete' and committed:
                        phase, sequence, cursor, dirty = 'retired', 1, 1, True
                    elif operation in ('reserve', 'reject_import') and committed:
                        sequence, watermark = 1, 131
                    elif operation == 'import':
                        sequence, watermark = (2 if committed else 1), 131
                        if committed:
                            phase, cursor, dirty = 'imported', 1, True
                    elif operation == 'undo':
                        phase, sequence, cursor, dirty = ('linked', 2, 0, False) if committed else ('edited', 1, 1, True)
                    elif operation == 'redo':
                        phase, sequence, cursor, dirty = ('edited', 3, 1, True) if committed else ('linked', 2, 0, False)
                    inspect(path, mode, phase, sequence, watermark, cursor, dirty)
                    if operation in ('reserve', 'reject_import'):
                        assigned = run(path, mode, 'allocate')
                        assert assigned['phase'] == 'expanded' and assigned['new_id'] == watermark + 1
                        assert assigned['watermark'] == watermark + 1 and assigned['sequence'] == sequence + 2
                        checks += 1
                    elif cursor:
                        history = run(path, mode, 'history')
                        assert history['phase'] == phase and history['watermark'] == watermark
                        assert history['cursor'] == cursor and history['sequence'] == sequence + 2
                        checks += 1
                    scenarios += 1

            # A completely rejected import still leaves its already flushed reservation durable.
            path = fixtures / f'{mode}-rejected-import-complete'
            run(path, mode, 'create')
            run(path, mode, 'reject_import')
            inspect(path, mode, 'linked', 1, 131, 0, False)
            next_value = run(path, mode, 'allocate')
            assert next_value['new_id'] == 132 and next_value['phase'] == 'expanded'
            checks += 1
            scenarios += 1

            save_points = ['after_base_flush', 'before_replace', 'after_replace', 'after_publish', 'before_cleanup']
            if mode == 'sidecar':
                save_points.insert(0, 'after_sidecar_flush')
            for point in save_points:
                path = fixtures / f'{mode}-save-{point}'
                run(path, mode, 'create')
                run(path, mode, 'reject_import')
                run(path, mode, 'edit')
                run(path, mode, 'save', point, expected=86)
                published = point in ('after_replace', 'after_publish', 'before_cleanup')
                inspect(path, mode, 'edited', 2, 131, 1, not published)
                history = run(path, mode, 'history')
                assert history['phase'] == 'edited' and history['watermark'] == 131
                assert history['sequence'] == 4 and history['cursor'] == 1
                checks += 1
                scenarios += 1

            for point in ('before_tail_repair', 'after_tail_repair'):
                path = fixtures / f'{mode}-repair-{point}'
                run(path, mode, 'create')
                run(path, mode, 'delete', 'after_payload', 1, expected=86)
                run(path, mode, 'recover', point, expected=86)
                inspect(path, mode, 'linked', 0, 130, 0, False)
                run(path, mode, 'edit')
                inspect(path, mode, 'edited', 1, 130, 1, True)
                scenarios += 1

    report = {'status': 'passed', 'profile': args.profile, 'checks': checks,
              'scenarios': scenarios, 'fresh_processes': len(commands), 'storage_modes': list(modes),
              'helper_sha256': hashlib.sha256(helper.read_bytes()).hexdigest(),
              'source_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'independent_framing': 'existing independent CRC64/footer/chain reader',
              'elapsed_seconds': round(time.monotonic() - started, 3),
              'limits': ['Abrupt process termination is qualified; power loss and provider hardware durability are separate.']}
    (directory / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
