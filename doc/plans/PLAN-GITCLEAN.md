# Git History Cleanup Plan

**Status:** Pending (execute after v0.7.0 merged to main)

## Context

This project had rapid early iteration resulting in:
- Premature version tagging (v1.0.0, v2.0.0 before production-ready)
- Noisy commit history (12x "Update README.md", 7x "Update touch-timeout.c")
- Documentation/implementation mismatch

Repo was made private during cleanup. Two known stargazers (imccausl, davellanedam) will be notified of changes.

## Version Scheme Change

| Old Tag | New Tag | Commit SHA | Notes |
|---------|---------|------------|-------|
| v0.2.0 | v0.2.0 | 2551b7e | Keep |
| v1.0.0 | v0.3.0 | 88d200d | Rename - not production-ready |
| v2.0.0 | v0.4.0 | 8bea5c2 | Rename - still maturing |
| PR #10 | v0.5.0 | f919d54 | Performance UX |
| PR #11 | v0.6.0 | 93cff29 | Docs & deployment |
| refactor | v0.7.0 | (pending) | Architecture simplification |
| (future) | v1.0.0 | - | First production-ready |

## Plan Overview

| Phase | Task | Status |
|-------|------|--------|
| 1 | Make repo private | Done |
| 2 | Release v0.6.0 | Done |
| 3 | Complete v0.7.0 refactor | In Progress |
| 4 | Merge v0.7.0 to main, tag | Pending |
| 5 | Update v0.5 release notes (add disclaimer) | Pending |
| 6 | Squash old history noise | Pending (optional) |
| 7 | Re-tag old versions | Pending |
| 8 | Make repo public | Pending |

## Phase 5-8 Details (Post-v0.7.0)

**Prerequisites before starting:**
- v0.7.0 merged to main and tagged
- All feature branches merged
- Repo still private

### Phase 5: Update v0.5 Release Notes

Edit GitHub release for v0.5.0 to add disclaimer about version scheme change and documentation accuracy. Use `gh release edit`.

### Phase 6: Squash History (Optional)

**Goal:** Consolidate noisy incremental commits between releases into clean squashed commits.

**Decision factors for skipping:**
- Time investment vs. benefit
- Preserving learning record may be valuable
- Phase 7 can proceed independently

**If proceeding:** Interactive rebase from initial commit, squash between release boundaries, force push. Create backup branch first.

### Phase 7: Re-tag Versions

1. Delete old tags locally and on remote: v1.0.0, v2.0.0
2. Create new tags at same commits: v0.3.0, v0.4.0
3. Delete old GitHub Releases, create new ones with correct tags

**Key SHAs:**
- v1.0.0 → v0.3.0: commit 88d200d
- v2.0.0 → v0.4.0: commit 8bea5c2

### Phase 8: Make Public

Restore visibility with `gh repo edit --visibility public`. Notify stargazers about version scheme change.
