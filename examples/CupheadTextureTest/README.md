# Cuphead Texture Pack Test

Minimal Solar Launcher test pack for Cuphead Wii U (`0005000021000000`).

This template intentionally contains **no Cuphead game assets**. The tester must provide a legally extracted Cuphead content file from their own installation.

## Goal

Validate that Solar detects `textures/` as a Cuphead Texture Pack and redirects the selected replacement file into `/vol/content`.

## SD path

```text
SD:/wiiu/SolarLauncher/games/0005000021000000/CupheadTextureTest/
├── mod.json
└── textures/
    └── <same relative path as the original file under /vol/content>
```

## Recommended test

1. Choose one **non-critical visual file** from the user's extracted Cuphead `content` directory.
2. Keep its exact relative path.
3. Make an obvious visual modification to the replacement copy (for example a large SOLAR TEST mark or a strong colour change).
4. Put only that replacement copy in `textures/`.
5. Enable `Cuphead Texture Pack Test` in Solar.
6. Launch Cuphead and visit the screen/scene that uses the selected file.
7. Disable the pack and relaunch to confirm the original resource returns.

Do not test with `Unity-master.rpx`, save data, or a large bundle as the first visual test.

## Expected Solar log

Solar should report the pack as a Cuphead texture pack and register a content redirection layer. If the visual replacement does not appear, preserve `SD:/wiiu/SolarLauncher/logs/solar.log` for diagnosis.
