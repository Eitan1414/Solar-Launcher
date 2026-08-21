# Cuphead Combined Texture + Behavior Test

Use this only **after** the standalone Texture Pack and Behavior Pack tests have both worked independently.

## Goal

Validate that Solar can enable Cuphead Texture and Behavior payloads in the same mod and keep their priority/conflict bookkeeping coherent.

## Structure

```text
CupheadCombinedPackTest/
├── mod.json
├── textures/
│   └── <user-supplied visual replacement at original relative path>
└── behavior/
    └── <user-supplied behavior replacement at original relative path>
```

This example ships without any Cuphead files.

## Procedure

1. Copy the already-proven Texture Test replacement into `textures/`.
2. Copy the already-proven Behavior Test replacement into `behavior/`.
3. Disable the two standalone test mods.
4. Enable only `Cuphead Combined Pack Test`.
5. Confirm both the visual change and behavior change are present in the same session.
6. Then deliberately create a duplicate target in another enabled mod to verify Solar reports a conflict and resolves it according to priority.

Always save `SD:/wiiu/SolarLauncher/logs/solar.log` after the run.
