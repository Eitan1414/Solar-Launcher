# Solar Launcher mod format (draft v0.2)

Solar V0.2 discovers mods by the currently running Wii U **Title ID** and can apply file replacement layers through Aroma's ContentRedirectionModule.

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

`content/` is merged into `/vol/content` and `aoc/` is merged into `/vol/aoc`.

`patches/` and `addons/` are reserved for later Solar versions.

## `mod.json`

```json
{
  "name": "Example Mod",
  "author": "Example Author",
  "version": "1.0.0",
  "titleId": "00050000XXXXXXXX",
  "type": "replacement",
  "enabled": true,
  "priority": 100
}
```

### Fields

- `name` — display name. Falls back to the folder name when omitted.
- `author` — mod author.
- `version` — mod version.
- `titleId` — 16-digit Wii U Title ID. When present, Solar checks it against the running title.
- `type` — metadata describing the mod. Planned values include `replacement`, `patch`, `addon`, and `total_mod`.
- `enabled` — whether the mod should be applied automatically. Defaults to `true`.
- `priority` — layer priority. Higher values override lower values when two mods provide the same file. Defaults to `0`.

## Layer priority

Solar adds lower-priority layers first and higher-priority layers last because ContentRedirection processes layers in reverse adding order.

Example:

```text
Base game
   ↓
Priority 0  - HD Texture Pack
   ↓
Priority 50 - Custom Music
   ↓
Priority 100 - Custom Character
```

If both priority `0` and priority `100` provide the same file, priority `100` wins.

## SDCafiine compatibility

Solar V0.2 also scans the standard Aroma SDCafiine location:

```text
SD:/wiiu/sdcafiine/
└── 00050000XXXXXXXX/
    └── ExistingPack/
        ├── content/
        └── aoc/
```

If exactly one compatible legacy SDCafiine pack exists for the running title, Solar V0.2 auto-enables it with a low priority so native Solar mods can override it.

If multiple legacy SDCafiine packs exist, Solar detects them but leaves them disabled because V0.2 does not yet include the pre-launch pack selector. This avoids choosing an arbitrary pack.

## File deletion marker

ContentRedirection merge layers support the same deletion marker used by the underlying module. To hide an original game file, place an empty file prefixed with `.deleted_` in the corresponding directory of the mod layer.

Example:

```text
content/music/.deleted_track1.wav
```

This is an advanced feature and should only be used when the target game's file layout is understood.
