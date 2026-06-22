#!/bin/bash
cd /mnt/e/work/opendlms/opendlms

echo "=== Finding original full-rework HEAD ==="
# The commit-map shows old->new mappings
# Original full-rework HEAD was 88ca3cb (before filter-repo)
# Its parent was 2616361 (the GOST crypto commit)

# Try to find the parent commit
echo "Checking parent candidates..."
for hash in $(git fsck --unreachable --no-reflogs 2>&1 | grep "^unreachable commit" | awk '{print $3}'); do
    tree_count=$(git ls-tree "$hash" --name-only 2>/dev/null | wc -l)
    if [ "$tree_count" -gt 3 ]; then
        echo "FOUND: $hash has $tree_count entries"
        git ls-tree "$hash" --name-only
    fi
done

echo ""
echo "=== Trying to recover blobs directly ==="
# Find the CMakeLists.txt blob by looking for blobs with the project declaration
echo "Searching for CMakeLists.txt blob..."
for hash in $(git fsck --unreachable --no-reflogs 2>&1 | grep "^unreachable blob" | awk '{print $3}'); do
    content=$(git cat-file -p "$hash" 2>/dev/null | head -1)
    if echo "$content" | grep -q "cmake_minimum_required\|project.*opendlms"; then
        echo "FOUND CMakeLists.txt blob: $hash"
        git cat-file -p "$hash" | head -3
        break
    fi
done
