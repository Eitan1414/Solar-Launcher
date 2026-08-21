# Cuphead Behavior Pack Test

Minimal Solar Launcher test pack for Cuphead Wii U (`0005000021000000`).

This template intentionally contains **no Cuphead game files**. The tester must provide their own legally extracted file.

## Goal

Validate that Solar detects `behavior/` as a Cuphead Behavior Pack and redirects a modified game-behavior file into `/vol/content`.

## SD path

```text
SD:/wiiu/SolarLauncher/games/0005000021000000/CupheadBehaviorTest/
├── mod.json
└── behavior/
    └── <same relative path as the original file under /vol/content>
```

## First recommended target

Use a **copy** of the game's `Assembly-CSharp.dll` from the user's own Cuphead extraction and change only one very small, easy-to-observe gameplay value. The exact path used inside `behavior/` must mirror the original path under `/vol/content`.

The first test should avoid broad rewrites, save-format changes, progression changes or anything that could corrupt persistent data.

## Test procedure

1. Keep a clean backup of the original file.
2. Make one minimal reversible change in the copy.
3. Place that copy under `behavior/` at the exact original relative path.
4. Enable only `Cuphead Behavior Pack Test` in Solar for the first run.
5. Launch Cuphead and check for the intended gameplay change.
6. Disable the pack and relaunch to confirm vanilla behavior returns.
7. Preserve `SD:/wiiu/SolarLauncher/logs/solar.log` whether the test succeeds or fails.

If the modified assembly crashes before reaching gameplay, the next iteration should reduce the change further rather than treating the redirection system itself as failed.
