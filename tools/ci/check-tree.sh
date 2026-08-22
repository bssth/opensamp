#!/usr/bin/env bash
#
# OpenSAMP tree guard.
#
# The authoritative version of tools/git-hooks/pre-commit. The hook is opt-in
# per clone (`git config core.hooksPath tools/git-hooks`), so it catches
# mistakes only for contributors who enabled it; this script runs in CI over
# the whole tracked tree and is the check that actually has to pass.
#
# Run it locally exactly the way CI does:
#
#     bash tools/ci/check-tree.sh
#
set -uo pipefail

status=0
max_size_bytes=5242880   # 5 MiB

fail() {
    # $1 = heading, $2 = offending paths, one per line.
    #
    # Deliberately takes its body as an argument rather than on stdin. A
    # function on the right-hand side of a pipe runs in a subshell, so its
    # assignment to `status` would be discarded — every gate below would print
    # its failures and the script would still exit 0.
    printf 'FAIL: %s\n' "$1"
    printf '%s\n' "$2" | sed 's/^/    /'
    printf '\n'
    status=1
}

pass() { printf 'ok:   %s\n' "$1"; }

tracked=$(git ls-files)

# Files scanned for content. Vendored code is exempt: it is shipped exactly as
# upstream wrote it. This script is exempt from its own patterns.
scan=$(printf '%s\n' "$tracked" | grep -vE '^native/vendor/|^tools/ci/check-tree\.sh$')

grep_tracked() {
    # $1 = extended regex. Prints matching tracked files, binaries skipped.
    printf '%s\n' "$scan" | tr '\n' '\0' | xargs -0 -r grep -IlE "$1" 2>/dev/null
}

# ---- 1. Proprietary game assets --------------------------------------------
# bin/ is a real GTA SA install. Publishing Rockstar's files in a public
# repository is the one mistake here that cannot be walked back.

asset_re='(^|/)gta_sa\.exe$|\.([Ii][Mm][Gg]|[Dd][Ff][Ff]|[Tt][Xx][Dd]|[Ii][Pp][Ll]|[Ii][Dd][Ee]|[Cc][Oo][Ll]|[Ii][Ff][Pp]|[Ss][Cc][Mm]|[Gg][Xx][Tt])$'
hits=$(printf '%s\n' "$tracked" | grep -E "$asset_re")
if [ -n "$hits" ]; then
    fail "tracked files look like GTA SA game data" "$hits"
else
    pass "no GTA game assets tracked"
fi

# ---- 2. Directories that are never tracked ---------------------------------

hits=$(printf '%s\n' "$tracked" | grep -E '^(bin|redist)/')
if [ -n "$hits" ]; then
    fail "tracked files under bin/ or redist/" "$hits"
else
    pass "bin/ and redist/ are untracked"
fi

# ---- 3. Oversized files ----------------------------------------------------
# Build junk and installers do not belong in history, and git never forgets.

hits=$(
    printf '%s\n' "$tracked" | while IFS= read -r file; do
        [ -f "$file" ] || continue
        size=$(wc -c < "$file")
        [ "$size" -gt "$max_size_bytes" ] && printf '%s (%s KiB)\n' "$file" "$((size / 1024))"
    done
)
if [ -n "$hits" ]; then
    fail "files over $((max_size_bytes / 1024)) KiB" "$hits"
else
    pass "no file over $((max_size_bytes / 1024)) KiB"
fi

# ---- 4. Leaked source trees and personal paths -----------------------------
# The offset provenance policy names public reverse-engineering references and
# nothing else. A local path additionally leaks a workstation layout into a
# public repository, and rots the moment anyone else clones.

hits=$(grep_tracked '[A-Za-z]:[\\/](dev|Users)[\\/]|samp-master|mtasa-blue-master')
if [ -n "$hits" ]; then
    fail "references to a local path or a leaked source tree" "$hits"
    printf '    Offsets are attributed to public references, not to a source tree,\n'
    printf '    and paths must be relative. See docs/offsets.md and CONTRIBUTING.md.\n\n'
else
    pass "no local paths or leaked-tree references"
fi

# ---- 5. Placeholders that must not ship ------------------------------------

hits=$(grep_tracked 'TODO: fill in|FIXME before (release|public)|MUST be removed before')
if [ -n "$hits" ]; then
    fail "unfilled placeholders" "$hits"
else
    pass "no unfilled placeholders"
fi

printf '\n'
if [ "$status" -eq 0 ]; then
    printf 'Tree guard passed.\n'
else
    printf 'Tree guard FAILED.\n'
fi
exit "$status"
