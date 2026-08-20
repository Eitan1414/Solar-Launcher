# Solar Launcher mod format (draft v0.4)

Solar V0.4 discovers mods by the currently running Wii U **Title ID** and can combine file replacement layers with memory/native patch payloads.

## Folder layout

```text
SD:/wiiu/SolarLauncher/
└── games/
    └── 00050000XXXXXXXX/
        └── ExampleMod/
            ├── mod.json
            ├── content/
            ├── aoc/
            ├── patches/
            └── addons/
```

- `content/` is merged into `/vol/content`.
- `aoc/` is merged into `/vol/aoc`.
- `patches/` contains Solar V0.4 declarative patch files (`*.json`).
- `addons/` remains reserved for a later Solar Addon/Game API version.

A mod may contain only one payload type or combine several of them.

## `mod.json`

```json
{
  "name": "Example Mod",
  "author": "Example Author",
  "version": "1.0.0",
  "titleId": "00050000XXXXXXXX",
  "type": "total_mod",
  "enabled": true,
  "priority": 100
}
```

### Fields

- `name` — display name. Falls back to the folder name when omitted.
- `author` — mod author.
- `version` — mod version.
- `titleId` — 16-digit Wii U Title ID. When present, Solar checks it against the running title.
- `type` — metadata describing the mod. Suggested values: `replacement`, `patch`, `addon`, `total_mod`.
- `enabled` — default enabled state. Defaults to `true`.
- `priority` — mod priority. Defaults to `0`.

The V0.3/V0.4 pre-launch menu can override `enabled` and `priority` per game without modifying `mod.json`.

## File layer priority

Solar adds lower-priority file layers first and higher-priority layers last because ContentRedirection processes layers in reverse adding order.

```text
Base game
   ↓
Priority 0   - HD Texture Pack
   ↓
Priority 50  - Custom Music
   ↓
Priority 100 - Custom Character
```

If priority `0` and priority `100` provide the same file, priority `100` wins.

## Patch priority

V0.4 also uses mod priority for overlapping memory patches.

Solar evaluates higher-priority memory patches first. If a lower-priority patch overlaps a range already claimed by a higher-priority patch, the lower-priority patch is skipped and logged.

Patch files live directly inside:

```text
MyMod/patches/*.json
```

See [`V0.4_PATCH_ENGINE.md`](V0.4_PATCH_ENGINE.md) for the complete patch format and safety rules.

## Native hooks

Patch files can request named native hooks. A hook only works if a compiled Solar Game Adapter registered the requested ID with `NativeHookRegistry`.

Solar does not execute arbitrary native binaries from a mod folder.

This is the mechanism intended for complex future mods such as the Cuphead 3-player test, after the exact Wii U RPX has been analyzed.

## SDCafiine compatibility

Solar also scans the standard Aroma SDCafiine location:

```text
SD:/wiiu/sdcafiine/
└── 00050000XXXXXXXX/
    └── ExistingPack/
        ├── content/
        └── aoc/
```

Legacy SDCafiine packs are file-replacement payloads only. They do not gain `patches/` support unless converted into a native Solar mod folder with a `mod.json`.

## File deletion marker

ContentRedirection merge layers support the underlying deletion marker. To hide an original game file, place an empty file prefixed with `.deleted_` in the corresponding layer directory.

```text
content/music/.deleted_track1.wav
```

This is an advanced feature and should only be used when the target game's file layout is understood.
