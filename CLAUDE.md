# Development Guide for Claude (and Humans)

**Project:** Nano SSH Server - World's smallest SSH server for microcontrollers

This document defines **MANDATORY** development practices. If you are Claude (AI assistant) or a human developer working on this project, you **MUST** follow these rules.

---

## Golden Rules

### 1. READ FIRST, CODE LATER

Before writing ANY code:

```bash
# REQUIRED reading order:
1. Read PRD.md (Product Requirements Document)
2. Read TODO.md (Task breakdown)
3. Read relevant docs/* for the task at hand
4. Understand the current phase and task
5. THEN start coding
```

**Violation:** Writing code without understanding requirements → STOP and read

### 2. NIX IS MANDATORY

**ALL development MUST happen inside `nix-shell`.**

```bash
# Start every session with:
cd /path/to/nano_ssh_server
nix-shell

# You should see nix-shell prompt
[nix-shell:~/nano_ssh_server]$
```

**Why:**
- Reproducible builds
- Consistent dependencies
- Works on any Linux/macOS

**Violations:**
- ❌ Installing system packages (apt, brew, etc.)
- ❌ Using system gcc/make
- ❌ "It works on my machine"
- ✅ Everything in shell.nix

### 3. JUSTFILE IS THE INTERFACE

**NEVER run raw commands. ALWAYS use `just`.**

```bash
# Correct:
just build v0-vanilla
just test v0-vanilla
just run v0-vanilla

# Wrong:
cd v0-vanilla && make
./v0-vanilla/nano_ssh_server
```

**Why:**
- Consistent interface
- Self-documenting
- Prevents errors
- Automation-ready

**If a command isn't in justfile, ADD IT there first.**

### 4. TEST BEFORE OPTIMIZE

**This is the MOST IMPORTANT RULE.**

```
NO CODE proceeds to the next phase until ALL tests pass.
```

**Testing discipline:**

```bash
# After ANY change:
just test <version>

# Before committing:
just test-all

# Before optimization:
just test <current-version>  # Must be GREEN

# After optimization:
just test <optimized-version>  # Must STILL be GREEN
```

**Violations that are FORBIDDEN:**
- ❌ "I'll add tests later"
- ❌ "Tests are failing but optimization looks good"
- ❌ "One test failing is okay"
- ❌ Skipping valgrind because "it's slow"

**Required test gates:**

| Phase | Gate | Criteria |
|-------|------|----------|
| After each component | Unit tests | Pass |
| After integration | Real SSH client test | "Hello World" printed by SSH client |
| Before git commit | Real SSH + All tests | SSH client prints "Hello World" + 100% pass |
| Before optimization | Memory test | Zero leaks (valgrind) |
| Before next phase | Full suite | All green + Real SSH client works |

**CRITICAL: Real SSH Client Testing**

You MUST test with a real SSH client (using sshpass) before ANY git commit:

```bash
# MANDATORY before every commit:
just test <version>              # Automated tests
echo "password123" | sshpass ssh -p 2222 user@localhost  # Real SSH client

# You MUST see the SSH client print:
# Hello World

# If you don't see "Hello World" printed: DO NOT COMMIT
```

### 5. SIZE MATTERS

**Measure size after EVERY change in optimization phases.**

```bash
# After any code change in v2+ versions:
just size-report

# Document the change:
echo "v3-opt2: 45678 bytes (-12% from v2)" >> size_log.txt
```

**Track:**
- Binary size (bytes)
- Change from previous version
- What optimization caused the change

---

## Project Structure (READ-ONLY REFERENCE)

```
nano_ssh_server/
├── PRD.md              ← Read this FIRST
├── TODO.md             ← Read this SECOND
├── CLAUDE.md           ← You are here
├── README.md           ← User-facing docs
├── shell.nix           ← Nix environment (MANDATORY)
├── justfile            ← Task runner (MANDATORY)
├── Makefile            ← Top-level orchestration
├── docs/               ← Implementation guides
│   ├── RFC4253_Transport.md
│   ├── RFC4252_Authentication.md
│   ├── RFC4254_Connection.md
│   ├── IMPLEMENTATION_GUIDE.md
│   ├── CRYPTO_NOTES.md
│   ├── TESTING.md
│   └── PORTING.md
├── v0-vanilla/         ← Phase 1: Working implementation
├── v1-portable/        ← Phase 2: Platform abstraction
├── v2-opt1/            ← Phase 3: Compiler optimizations
├── v3-opt2/            ← Phase 3: Crypto library optimization
├── v4-opt3/            ← Phase 3: Static buffers
├── v5-opt4/            ← Phase 3: State machine optimization
├── v6-opt5/            ← Phase 3: Aggressive optimization
└── tests/              ← Test scripts (MANDATORY to run)
```

---

## Development Workflow

### Starting a New Task

```bash
# 1. Enter nix-shell
nix-shell

# 2. Check TODO.md
cat TODO.md | grep "P0.*\[ \]" | head -5

# 3. Pick ONE task marked P0 and [ ]

# 4. Read relevant documentation
#    - If network task: read docs/RFC4253_Transport.md
#    - If auth task: read docs/RFC4252_Authentication.md
#    - If crypto task: read docs/CRYPTO_NOTES.md
#    - etc.

# 5. Write code for THAT ONE TASK ONLY

# 6. Build
just build <version>

# 7. Test
just test <version>

# 8. REAL SSH CLIENT TEST (MANDATORY)
# Start server: just run <version>
# In another terminal: echo "password123" | sshpass ssh -p 2222 user@localhost
# MUST see "Hello World" printed

# 9. If tests fail: FIX, don't proceed

# 10. If tests pass AND SSH client prints "Hello World": Mark task [x] in TODO.md

# 11. Commit (ONLY if real SSH client test passed)
git add .
git commit -m "Clear description of what was done"

# 11. Repeat
```

### Testing Workflow

**After every code change:**

```bash
# Quick test (specific version)
just test v0-vanilla

# Full test suite (before commit)
just test-all

# MANDATORY: Real SSH client test (MUST pass before commit)
just run v0-vanilla
# In another terminal:
echo "password123" | sshpass ssh -p 2222 user@localhost
# You MUST see "Hello World" printed by the SSH client

# Memory leak test (before phase completion)
just valgrind v0-vanilla
```

**CRITICAL: You MUST test with sshpass and real SSH client before making git commits.**

The SSH client MUST print "Hello World" to stdout. If you don't see this output, DO NOT commit.

**Test failure response:**

```bash
# DO NOT:
# - Disable the test
# - Skip the test
# - Comment out assertions
# - Blame the test
# - Proceed to next task

# DO:
# - Read the test output carefully
# - Use `just debug v0-vanilla` to investigate
# - Fix the actual bug
# - Re-run test
# - Ensure it passes
```

### Optimization Workflow

**ONLY after v0-vanilla is 100% working:**

```bash
# 1. Verify current version works
just test v0-vanilla
# Must be ALL GREEN

# 2. Measure baseline
just size-report

# 3. Copy to new version
cp -r v1-portable v2-opt1

# 4. Make ONE optimization change

# 5. Build
just build v2-opt1

# 6. Test IMMEDIATELY
just test v2-opt1

# 7. If broken: REVERT the optimization

# 8. If working: Measure size
just size-report

# 9. Document the change
echo "Changed X to Y: saved Z bytes" >> optimization_log.txt

# 10. Commit
git add .
git commit -m "v2-opt1: Describe optimization and size impact"
```

---

## Absolute Requirements

### Before ANY commit:

```bash
# Checklist:
[ ] Code compiles without warnings
[ ] All tests pass (just test-all)
[ ] REAL SSH CLIENT TEST PASSES (echo "password123" | sshpass ssh -p 2222 user@localhost)
[ ] SSH client prints "Hello World" to stdout
[ ] TODO.md updated (tasks marked [x])
[ ] Commit message is clear
[ ] No temporary/debug files included
```

**MANDATORY: Every commit MUST be tested with a real SSH client first.**

### Before moving to next phase:

```bash
# Checklist:
[ ] ALL P0 tasks in current phase marked [x]
[ ] All tests passing (100%)
[ ] Valgrind reports zero leaks
[ ] Manual SSH client test successful
[ ] Size measured and documented
[ ] Code committed and pushed
```

### Before creating a PR:

```bash
# Checklist:
[ ] All versions build successfully
[ ] All tests pass for all versions
[ ] Size comparison table updated
[ ] README.md reflects current state
[ ] No TODOs or FIXMEs in code (unless documented in TODO.md)
[ ] Documentation is accurate
```

---

## Common Scenarios

### Scenario: "I want to add a feature"

```
1. STOP
2. Is it in PRD.md?
   - NO → Don't add it. This is a MINIMAL implementation.
   - YES → Continue
3. Is there a task in TODO.md?
   - NO → Create task in TODO.md first
   - YES → Continue
4. Follow development workflow above
```

### Scenario: "Tests are failing but I want to continue"

```
1. STOP
2. You MUST fix tests before proceeding
3. No exceptions
4. Read docs/TESTING.md for debugging help
5. Use `just debug <version>` to investigate
6. Fix the bug
7. Tests must pass before ANY other work
```

### Scenario: "This optimization makes it smaller but breaks a test"

```
1. REVERT the optimization
2. Optimization is NOT successful if it breaks functionality
3. Find a different optimization approach
4. Or accept that this optimization doesn't work
```

### Scenario: "Nix is too slow / complicated"

```
1. Tough luck
2. Nix is mandatory
3. It's not about your convenience, it's about reproducibility
4. Future you (and others) will thank you
```

### Scenario: "I found a bug in the tests"

```
1. Great! Fix the test.
2. Document what was wrong
3. Make sure fix doesn't hide real bugs
4. Run tests again
5. All tests must still pass
```

### Scenario: "Can I use a different crypto library?"

```
1. Check docs/CRYPTO_NOTES.md
2. If library is listed there: Maybe
3. Measure the size impact
4. All tests must still pass
5. Document the change and size comparison
6. If no size benefit: Revert
```

---

## Code Quality Standards

### Correctness > Size (for v0-v1)

```c
// Phase 1: v0-vanilla
// GOOD:
if (packet_length > MAX_PACKET_SIZE) {
    fprintf(stderr, "Packet too large: %u\n", packet_length);
    return -1;
}

// BAD (too early optimization):
if (packet_length > MAX_PACKET_SIZE) return -1;  // No error message
```

### Size > Readability (for v3+)

```c
// Phase 3: v3-opt2+
// NOW it's okay:
if (packet_length > MAX_PACKET_SIZE) return -1;

// Even better (if it saves bytes):
return (packet_length > MAX_PACKET_SIZE) ? -1 : 0;
```

### Security

```c
// ALWAYS:
// - Clear sensitive data after use
memset(password, 0, sizeof(password));

// - Use constant-time comparison for secrets
crypto_verify_32(mac1, mac2);  // NOT memcmp()

// - Validate all inputs
if (len > MAX_SIZE) return -1;
```

### Comments

```c
// Phase 1: Comment complex logic
// Good:
// Exchange hash: H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)
compute_exchange_hash(...);

// Phase 3+: Remove comments to save space
// (But document in commit messages instead)
```

---

## Git Workflow

### Commit Messages

```bash
# GOOD:
git commit -m "Implement Curve25519 key exchange in v0-vanilla

- Generate ephemeral key pair
- Compute shared secret
- Build exchange hash H
- Sign with Ed25519 host key
- All tests pass

Task: TODO.md Phase 1.6"

# BAD:
git commit -m "fix stuff"
git commit -m "wip"
git commit -m "asdf"
```

### Commit Frequency

```
Commit after EACH completed task from TODO.md
```

- Too frequent: After every line → NO
- Just right: After each TODO task → YES
- Too infrequent: Once per day → NO

---

## Debugging Guide

### When something doesn't work:

```bash
# 1. Read the error message carefully
# 2. Check verbose SSH client output
ssh -vvv -p 2222 user@localhost

# 3. Use debugger
just debug v0-vanilla
# Then in gdb:
break main
run
step

# 4. Capture packets
sudo tcpdump -i lo -w capture.pcap port 2222
# Then in another terminal:
ssh -p 2222 user@localhost
# Analyze with:
wireshark capture.pcap

# 5. Add temporary debug output
printf("DEBUG: packet_length=%u\n", packet_length);
# (Remove before commit to optimized versions)

# 6. Consult docs
cat docs/IMPLEMENTATION_GUIDE.md | grep -A 10 "Common Pitfalls"

# 7. Read RFCs
cat docs/RFC4253_Transport.md

# 8. Compare with test vectors
# See docs/CRYPTO_NOTES.md for test vectors
```

---

## Phase-Specific Guidelines

### Phase 0: Setup

**Goal:** Get environment working

```bash
# Success criteria:
just --list  # Shows all commands
just build v0-vanilla  # Compiles (even if empty)
just test v0-vanilla  # Runs (even if no tests yet)
```

### Phase 1: v0-vanilla

**Goal:** Working SSH server, correctness first

**Rules:**
- Use standard libraries freely
- Prioritize readability
- Add extensive error checking
- Use dynamic allocation if easier
- Add debug output liberally
- Make it WORK, not small

**Success:** `ssh -p 2222 user@localhost` → "Hello World"

### Phase 2: v1-portable

**Goal:** Platform abstraction

**Rules:**
- No platform-specific code in core
- All platform details in `net_*.c` files
- Must work identically to v0-vanilla
- Small size increase is acceptable

**Success:** Same tests pass, compiles with `PLATFORM=posix`

### Phase 3: v2-v6 optimizations

**Goal:** Progressively smaller binaries

**Rules:**
- ONE optimization per version
- Test after EACH change
- Measure size after EACH change
- Document what worked and what didn't
- Never break functionality for size

**Success:** Smaller binary, all tests still pass

---

## What to Do When Stuck

### "I don't understand the SSH protocol"

```bash
# Read in order:
1. docs/IMPLEMENTATION_GUIDE.md (high-level flow)
2. docs/RFC4253_Transport.md (specific section)
3. Test with: ssh -vvv -p 2222 user@localhost
4. Compare with: actual SSH client verbose output
```

### "Crypto implementation is confusing"

```bash
1. Read: docs/CRYPTO_NOTES.md
2. Use TweetNaCl (simplest API)
3. Test with known vectors first
4. Don't write crypto from scratch (in v0-v1)
```

### "Tests are failing and I don't know why"

```bash
1. Read: docs/TESTING.md
2. Run with: just debug v0-vanilla
3. Use: ssh -vvv to see client perspective
4. Capture packets: tcpdump + wireshark
5. Compare: your packet vs expected format
```

### "Binary is too large"

```bash
# Not your problem in Phase 1-2!
# Wait until Phase 3
# Then read: PRD.md section "Size Optimization Strategies"
```

---

## Success Metrics

### Phase 1: v0-vanilla

- [ ] Compiles without errors
- [ ] Compiles without warnings
- [ ] All unit tests pass
- [ ] Integration test passes
- [ ] `ssh -p 2222 user@localhost` works
- [ ] Receives "Hello World"
- [ ] Valgrind: zero leaks
- [ ] No crashes on disconnect
- [ ] Binary size: Don't care (just get it working)

### Phase 2: v1-portable

- [ ] All Phase 1 metrics still met
- [ ] No platform-specific code in core
- [ ] Compiles with different PLATFORM flags
- [ ] Binary size: < 200KB acceptable

### Phase 3: Optimizations

- [ ] All functionality preserved
- [ ] All tests still pass
- [ ] Each version smaller than previous
- [ ] Target: v6-opt5 < 32KB (stretch goal)

---

## Red Flags (STOP and reconsider)

🚩 Disabling tests to make build pass
🚩 "TODO: Fix this later" comments
🚩 Skipping valgrind because "it takes too long"
🚩 Installing dependencies outside nix-shell
🚩 Running commands instead of using justfile
🚩 Optimizing before tests pass
🚩 Moving to next phase with failing tests
🚩 Adding features not in PRD.md
🚩 Changing protocol to "make it easier"
🚩 "This test is wrong" (without proof)
🚩 Committing with "wip" message
🚩 Not reading documentation first
🚩 **Committing without testing with real SSH client (sshpass)**
🚩 **Committing without seeing "Hello World" printed by SSH client**

---

## Summary: Your Development Checklist

**Every session:**
- [ ] `nix-shell` (enter environment)
- [ ] `cat TODO.md` (check next task)
- [ ] Read relevant docs
- [ ] Implement ONE task
- [ ] `just build <version>`
- [ ] `just test <version>`
- [ ] **Test with real SSH client (sshpass) - MUST see "Hello World"**
- [ ] Mark task `[x]` in TODO.md
- [ ] `git commit` with clear message (ONLY after real SSH test passes)
- [ ] Repeat

**Every phase completion:**
- [ ] ALL P0 tasks done
- [ ] ALL tests passing
- [ ] Valgrind clean
- [ ] Size measured and documented
- [ ] Manual SSH test successful
- [ ] Everything committed
- [ ] Move to next phase

**Remember:**
1. Read PRD.md first
2. Use nix-shell always
3. Use just for all commands
4. Test before optimize
5. Never skip tests
6. One task at a time
7. Commit frequently
8. Measure size in Phase 3+

---

## Final Words

This project is about discipline:

- **Discipline to read** before coding
- **Discipline to test** before proceeding
- **Discipline to measure** every change
- **Discipline to commit** frequently
- **Discipline to follow** the process

The goal is not just a small SSH server.

The goal is a **provably correct** small SSH server that **we can trust** through **rigorous testing** and **reproducible builds**.

If you (Claude or human) follow this guide, you will succeed.

If you skip steps, you will waste time debugging mysterious issues.

**Choose wisely.**

---

*Last updated: 2024-01-15*
*See PRD.md for project requirements*
*See TODO.md for task breakdown*
