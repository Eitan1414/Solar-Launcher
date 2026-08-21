# Cuphead Texture / Behavior Compatibility Test Plan

Target:

```text
Game: Cuphead Wii U
Title ID: 0005000021000000
Solar branch: solar-launcher-v0.5.1-polish
```

The test suite is split into three stages so a failure can be isolated quickly.

## Test A — Texture Pack

Example: `examples/CupheadTextureTest`

Expected launcher payload:

```text
T = yes
B = no
```

Pass criteria:

- Solar detects the mod
- the pack can be enabled/disabled
- no conflict is reported when it is the only pack targeting that path
- the selected visual resource changes in-game
- disabling the mod restores the original resource
- no unexpected crash/reopen loop occurs

## Test B — Behavior Pack

Example: `examples/CupheadBehaviorTest`

Expected launcher payload:

```text
T = no
B = yes
```

Pass criteria:

- Solar detects the behavior pack
- the replacement file is mounted at the expected `/vol/content` target
- the chosen minimal gameplay change is visible
- disabling the pack restores vanilla behavior
- saves/progression are not altered by the test

A crash from a malformed or incompatible modified `Assembly-CSharp.dll` does not by itself prove Solar redirection failed. Check `solar.log` to separate replacement-layer success from managed-code validity.

## Test C — Combined Pack

Example: `examples/CupheadCombinedPackTest`

Expected launcher payload:

```text
T = yes
B = yes
```

Pass criteria:

- both changes are visible in one launch
- the pack can be toggled as one unit
- priority is respected
- deliberate duplicate target paths are reported by conflict detection

## Log checklist

After each run copy:

```text
SD:/wiiu/SolarLauncher/logs/solar.log
```

Useful evidence includes:

- detected mod name
- detected Texture/Behavior flags
- registered redirection roots
- conflict count
- enabled priority
- any ContentRedirection or Patch Engine failure

## Safety

- Work only from user-owned/legal game files.
- Keep original files outside the Solar mod directory as backups.
- Start with one replacement file per test.
- Avoid save files and broad assembly edits in the first pass.
- Do not run standalone SDCafiine in parallel with Solar for the same Cuphead replacement test.
