#!/bin/bash
cd /mnt/e/work/opendlms/opendlms

echo "=== Searching unreachable commits for full tree ==="
for hash in $(git fsck --unreachable --no-reflogs 2>/dev/null | grep "^unreachable commit" | awk '{print $3}'); do
    count=$(git ls-tree "$hash" --name-only 2>/dev/null | wc -l)
    echo "$hash: $count top-level entries"
done

echo ""
echo "=== Trying to restore from unreachable commits ==="
# Try each unreachable commit to see if it has the full tree
for hash in $(git fsck --unreachable --no-reflogs 2>/dev/null | grep "^unreachable commit" | awk '{print $3}'); do
    count=$(git ls-tree "$hash" --name-only 2>/dev/null | wc -l)
    if [ "$count" -gt 5 ]; then
        echo "Found commit with $count entries: $hash"
        git ls-tree "$hash" --name-only
        echo ""
    fi
done
