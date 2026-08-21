# Cuphead packs in Solar Launcher

Solar Launcher v0.5.1-polish recognizes two Cuphead-specific convenience payloads for title `0005000021000000`.

## Texture Pack

Canonical folder:

```text
textures/
```

Legacy/alternate alias also accepted:

```text
texture_pack/
```

The folder mirrors the game's `/vol/content` hierarchy. Solar mounts it as a normal ContentRedirection merge layer, so only files present in the pack replace the original game files.

Example:

```text
MyCupheadTexturePack/
├── mod.json
├── icon.png
└── textures/
    └── <same relative path as the target file under /vol/content>
```

Recommended manifest type:

```json
{
  "name": "My Cuphead Texture Pack",
  "author": "Author",
  "version": "1.0",
  "type": "texture-pack",
  "titleId": "0005000021000000",
  "enabled": true,
  "priority": 0
}
```

## Behavior Pack

Canonical folder:

```text
behavior/
```

Legacy/alternate alias also accepted:

```text
behavior_pack/
```

This folder also mirrors `/vol/content`, but is intended for files that change Cuphead's managed/gameplay behavior, for example modified Mono-managed files from the user's own legally obtained Cuphead installation.

Example:

```text
MyCupheadBehaviorPack/
├── mod.json
├── icon.png
└── behavior/
    └── <same relative path as the target file under /vol/content>
```

Behavior packs can still include Solar's normal top-level `patches/` folder when runtime memory patches or native hooks are needed in addition to file replacement.

## Mixing pack types

A single mod may contain both payloads:

```text
CupheadTotalMod/
├── mod.json
├── icon.png
├── textures/
├── behavior/
└── patches/
```

Solar applies the existing mod priority system to all layers. Texture and behavior paths are compared against ordinary `content/` mods by the conflict detector because all three ultimately target `/vol/content`.

In the launcher payload display:

```text
C = content
T = texture pack
B = behavior pack
A = add-on content (AOC)
P = patches/hooks
```

Solar does not distribute Cuphead game assets. Pack creators and users should provide files derived from their own legally obtained game copy.
