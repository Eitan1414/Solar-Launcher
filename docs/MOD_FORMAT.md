# Solar Launcher mod format (draft v0.1)

Solar V0.1 discovers mods by the currently running Wii U **Title ID**.

## Folder layout

```text
SD:/wiiu/SolarLauncher/
└── games/
    └── 00050000XXXXXXXX/
        └── ExampleMod/
            ├── mod.json
            ├── content/
            ├── patches/
            └── addons/
```

At this stage, only discovery and metadata parsing are implemented. `content/`, `patches/`, and `addons/` are reserved for later Solar versions.

## `mod.json`

```json
{
  "name": "Example Mod",
  "author": "Example Author",
  "version": "1.0.0",
  "titleId": "00050000XXXXXXXX",
  "type": "replacement"
}
```

### Fields

- `name` — display name. Falls back to the folder name when omitted.
- `author` — mod author.
- `version` — mod version.
- `titleId` — 16-digit Wii U Title ID. When present, Solar V0.1 checks it against the running title.
- `type` — currently metadata only. Planned values include `replacement`, `patch`, `addon`, and `total_mod`.

The manifest format is intentionally small while the Solar Core is being stabilized.
