# Project History: A Case Study in AI-Assisted Development

This document chronicles the development of touch-timeout as a case study in non-expert AI-assisted software engineering. I attempted to "fly the plane while building it," using the AI as a software engineering consultant, giving it minimal and non-expert guidance, learning along the way. While this got me a working prototype fast, it came with significant risks and pitfalls, described below. I believe these learnings could be generalized to almost any expert domain. When AI is used as an expert consultant, i.e. the user has no means to verify the output and properly engineer the input (context), there are significant risks, the consequence dependent on the application. Because the output may 'work' from the user's perspective, the risks may be hidden, making them even more dangerous. The user may have a false sense of security. 

This experiment highlights the importance of expert verification of AI-assisted output. When non-experts use AI as an expert consultant, good input (context engineering) can mitigate the risks, but the least risky approach to using AI is still the delegation approach (likely the design intent of AI tools) where an expert curates (engineers) the input (context) and verifies the output. Good context engineering is no less important in both cases: to mitigate risk from the output in the consultant use case, and to minimize wasted time and effort in the delegation use case. 

## Context

**Project:** Touchscreen backlight timeout daemon for Raspberry Pi
**Developer:** Non-expert learning software engineering
**AI Tools:** Claude (web interface), Claude Code
**Timeline:** November 2024 – December 2025

## The Experiment

**Goal:** Use AI assistance to build a production-quality embedded C daemon while learning software engineering principles. How does the AI perform when used by a non-expert in a "consultant" use case.

**Hypothesis:** AI can accelerate development even for a non-expert developer.

**Result:** Yes, but with significant caveats. Without proper context engineering, AI produces technical debt faster than a human could alone. AI-assisted development does not eliminate the need for domain expertise, at least to the point that successful delegation is possible. 

## Timeline

### Phase 1: Web Interface Development (v0.1–v0.3)

**Approach:** Claude web interface + GitHub direct editing

**What happened:**
- Iterative development through conversation
- No local development environment
- Code changes made directly in GitHub web editor
- No architecture planning or design documents

**Outcome:** Working code, but:
- Messy, inconsistent style
- No clear separation of concerns
- Single-file implementation grew organically
- No tests

**Lessons:**
- Web-based iteration works for prototyping
- Without local tooling, code quality suffers
- "Working" is not the same as "maintainable"

### Phase 2: First Claude Code Refactor (v0.4)

**Approach:** Claude Code with minimal direction

**Context given to Claude Code:**
- The messy v0.3 monolithic codebase
- Vague instruction: "refactor this to be more modular"
- No design specification
- No architecture guidance

**What Claude produced:**
- 6-module architecture (~900 lines total)
- Event-driven I/O with `timerfd`
- Configuration file parser
- Unit test framework
- Cross-compilation support
- Remote deployment scripts

**Problems:**
- Over-engineered for the problem scope (e.g. HAL abstraction code bloat, table driven config parsing)
- Inconsistent error handling strategies
- Documentation written for intended behavior, not actual implementation
- Mixed abstraction levels
- Features added or persisted that weren't requested or needed (config file, systemd watchdog)
- Difficult to understand or modify

**Root cause:** Claude Code optimizes for "professional-looking" code. Given a messy codebase and no constraints, it produced what it "thought" production code should look like—comprehensive but inappropriate for a simple daemon.

### Phase 3: Documentation Cleanup (v0.5–v0.6)

**Approach:** Document what exists, stabilize before changing

**What happened:**
- Created ARCHITECTURE.md, DESIGN.md, INSTALLATION.md
- Reorganized into `doc/` directory
- Improved deployment scripts
- Added test UX improvements

**Problems discovered:**
- Documentation described intended behavior, not implementation
- 9 documented discrepancies found (see `doc/plans/documentation-issues.md`)
- API functions documented that didn't exist
- Test counts wrong
- Line number references outdated

**Lesson:** Documentation written alongside AI-generated code inherits the same blind spots. Always verify against implementation.

### Phase 4: Clean-Slate Rewrite (v0.7)

**Approach:** Design-first, then implement

**Process:**
1. Wrote DESIGN.md with explicit constraints and non-goals
2. Defined clear module boundaries (2 modules, not 6)
3. Specified interfaces before implementation
4. Deleted all v0.4 modules
5. Rebuilt from scratch with Claude Code following the spec

**Result:**
- 2 modules (~370 lines total)
- Pure state machine (testable, no I/O)
- Simple `poll()` loop (no `timerfd` complexity)
- CLI-only configuration (no config file parser)
- Code matches documentation

## Key Insights

### 1. AI Amplifies Your Process

If your process is "figure it out as you go," AI will help you figure it out faster—and create technical debt faster too.

If your process is "design first, then implement," AI will implement your design efficiently.

### 2. Context is Critical

**Bad context:**
- "Here's messy code, make it better"
- Vague goals, no constraints
- No design documents

**Good context:**
- Clear problem statement
- Explicit constraints and non-goals
- Design specification with interfaces
- Implementation guide with patterns to follow

### 3. AI Defaults to Over-Engineering

Without constraints, Claude Code tends to produce:
- More abstraction than necessary
- More features than requested
- "Professional-looking" patterns that may be inappropriate
- Comprehensive error handling even where unnecessary

**Mitigation:** Explicitly state what NOT to build. List non-goals. Specify simplicity as a requirement.

### 4. Documentation Doesn't Verify Itself

AI-generated documentation describes what the AI *intended* to build, not what it *actually* built. Always:
- Verify documentation against code
- Run the verification commands you document
- Test the examples you provide

### 5. Iteration Without Direction Compounds Debt

Each AI-assisted iteration without clear direction adds complexity. After several cycles:
- The codebase becomes harder to understand
- Documentation drifts further from implementation
- The AI has more "context" that's actually noise
- Fixing issues becomes harder than rewriting

## Metrics

| Metric | v0.4 (over-engineered) | v0.7 (design-first) |
|--------|------------------------|---------------------|
| Modules | 6 | 2 |
| Lines of code | ~900 | ~370 |
| Config sources | CLI + file | CLI only |
| Test complexity | Mocks required | Pure functions |
| Time to understand | Hours | Minutes |

## Future Work

This project will serve as the "before" example in a comparison study:

**Study design:**
1. **This project (touch-timeout):** AI development without context engineering
2. **Future project:** Same complexity, with proper design docs and implementation guide provided upfront

**Hypothesis:** Design-first approach with AI will produce better results in less time, with less technical debt.

## Recommendations

For developers using AI assistance:

1. **Write design docs first.** Even brief ones. Include constraints and non-goals.

2. **Be specific about what NOT to build.** AI will add features unless told not to.

3. **Verify documentation against code.** Run every command, test every example.

4. **Prefer rewriting over iterating.** If the foundation is wrong, more AI iterations make it worse.

5. **Keep context clean.** Messy context produces messy output.

## Technical Notes

### Performance Comparison: v0.4-0.6 → v0.7.0 (December 2025)

**Test date:** 2025-12-19 (single 30-second measurement per version)

The v0.7.0 rewrite shows improved resource usage compared to the v0.4-0.6 codebase:

| Metric | v0.4-0.6 | v0.7.0 | Notes |
|--------|----------|--------|-------|
| Memory (KB) | 1,376 | 360 | **74% reduction** |
| FD count | 7 | 5 | Removed timerfd |
| CPU | ~0.4% | ~0.4% | Both effectively idle |
| Memory growth | 0 | 0 | No leaks detected |
| SD writes | 0 | 0 | No writes during idle |

**Architectural changes driving improvements:**
- Removed timerfd (2 fewer FDs)
- Eliminated config file parser and HAL abstractions
- Reduced from 6 modules (~900 lines) to 2 modules (~370 lines)
- Simpler poll() loop with direct timeout calculation

**Caveats:** Single measurement session. CPU values are within noise floor for an idle daemon.

**v0.7.0 performance targets (met):**
- CPU ~0% ✓
- Memory <0.5MB ✓ (0.35MB actual)
- SD writes = 0 ✓
- FD delta = 0 ✓

### Memory Measurement on Linux 5.x (December 2024)

When measuring memory usage of the static binary on HifiBerryOS (Linux 5.15, Buildroot),
we discovered a discrepancy between standard tools and actual memory consumption:

| Source | Value | What it measures |
|--------|-------|------------------|
| `ps -o rss` / VmRSS | 4 KB | Anonymous pages only |
| `/proc/PID/smaps_rollup Rss` | 360 KB | All resident pages |

**Root cause:** On Linux 5.x with statically-linked binaries, the kernel's VmRSS
(and therefore `ps`, `top`, `htop`) only counts anonymous pages (heap, stack, dirty data).
It does NOT count Private_Clean file-backed pages from the executable's code section,
even though these pages ARE resident in physical RAM.

**Memory breakdown from smaps:**
```
Code section:  320 KB (Private_Clean, file-backed from binary)
Data section:   16 KB (8 KB Private_Dirty + 8 KB rodata)
Heap:           ~8 KB
Stack:         ~16 KB
Total Rss:    ~360 KB
```

**Why this matters:**
- Standard tools report 4 KB, but 360 KB of physical RAM is actually consumed
- The code pages ARE using RAM and would contribute to memory pressure
- The kernel could reclaim them (they're clean copies of the binary), but hasn't

**For performance testing:** Use `smaps_rollup` for accurate measurements.
See `scripts/test-performance.sh` for the implementation.

**Future optimization opportunity:** The 320 KB code section is large for a simple daemon.
This could be reduced by:
- Enabling `-Os` optimization instead of `-O2`
- Stripping more aggressively
- Using dynamic linking (trades binary size for shared library overhead)
- Reviewing if all code paths are necessary

## References

- [CHANGELOG.md](../CHANGELOG.md) — Version history with context
- [doc/plans/documentation-issues.md](plans/documentation-issues.md) — v0.4 discrepancies found
- [doc/DESIGN.md](DESIGN.md) — Design specification (v0.7+)
- [doc/ARCHITECTURE.md](ARCHITECTURE.md) — Implementation documentation (v0.7+)
