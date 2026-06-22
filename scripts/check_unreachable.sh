#!/bin/bash
cd /mnt/e/work/opendlms/opendlms
echo "=== Unreachable commits ==="
for c in 03cfdec4fc93dbc4c7f80f01168ae9752170630f d6a842a20fa45f41b28303064126fd7f66ee43ce ef76d021f65f3e8eed7960584b977e55e726cf27; do
    echo "--- $c ---"
    git ls-tree "$c" --name-only tests/ 2>/dev/null
    echo ""
done
