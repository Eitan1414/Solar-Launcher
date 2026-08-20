<p align="center">
  <img src="Assets/Solar-logo.png" width="500" alt="Solar Launcher">
</p>

<h1 align="center">☀️ Solar Launcher</h1>

<p align="center">
  <b>Universal Wii U modding framework for Aroma</b>
</p>

<p align="center">
  Load. Combine. Expand.
</p>

---

## About

**Solar Launcher** is an experimental universal modding framework for the **Wii U**, designed for the **Aroma** environment.

The goal is to provide one common system for multiple kinds of Wii U mods instead of requiring a completely different workflow for every project.

Solar is designed around two levels of modding:

1. **Universal modding** — file replacement, SDCafiine-style packs, mod selection, priorities and declarative memory patches.
2. **Game-specific modding** — advanced gameplay modifications powered by optional Game Adapters.

> ⚠️ Solar Launcher is still experimental. The code for V0.1–V0.4 exists on development branches, but the current V0.4 build has **not yet been fully compiled and validated on real Wii U hardware**.

---

# 🚧 Current Project Status

The project is currently at **Solar Launcher V0.4 — Patch Engine**.

Current development branch:

```text
solar-patchengine-v0.4
```

Solar is no longer only a concept: the project now contains a real WUPS/Aroma codebase with a Title Manager, Mod Manager, file redirection engine, pre-launch mod menu, conflict detection, per-game selections and the first Patch Engine implementation.

However, the project is **not yet considered stable or release-ready**.

## ✅ Implemented in the codebase

### V0.1 — Solar Core

- WUPS/Aroma plugin structure
- current Title ID detection
- Solar SD directory creation
- `mod.json` parsing
- per-title mod scanning
- logging and basic configuration

### V0.2 — File Mods

- `ContentRedirectionModule` integration
- `/vol/content` replacement layers
- `/vol/aoc` replacement layers
- multiple replacement mods
- priority ordering
- existing SDCafiine pack detection
- fallback to the original game file when no replacement exists

### V0.3 — Mod Management

- pre-launch Solar mod menu
- GamePad and Wii controller input support
- enable/disable mods before launch
- change mod priority
- per-game saved selections
- one-time vanilla launch option
- replacement-file conflict detection
- SDCafiine packs listed in the same selector

### V0.4 — Patch Engine

- `patches/` detection
- declarative JSON memory patches
- address validation
- expected-byte verification before writing
- protection against overlapping patches
- restoration of original bytes when the game closes
- `FunctionPatcherModule` integration
- Native Hook Registry foundation
- support for patch-only mods
- foundation for future Game Adapter hooks

## 🧪 Still needs validation

The code exists, but the following still needs real-world validation before Solar should be treated as stable:

- successful production compilation of `SolarLauncher.wps`
- boot test under Aroma on a real Wii U
- pre-launch menu test on TV and GamePad
- multiple simultaneous replacement-mod test
- SDCafiine compatibility test
- conflict-priority test
- memory patch application/restoration test
- invalid-address and wrong-expected-byte safety test
- FunctionPatcher availability and lifecycle test
- long-session stability testing

The current GitHub Actions workflow has not yet provided a validated V0.4 `.wps` artifact through the development flow being used here, so **do not treat V0.4 as a tested release yet**.

## ⏳ Not implemented yet

The following are planned, but do **not** exist as finished features yet:

- external Game Adapters loaded independently from Solar Core
- Cuphead Game Adapter
- Cuphead 3-player gameplay mod
- Cuphead 4-player gameplay mod
- automatic signature resolver
- Solar Auto Analyzer
- PC RPX/RPL Analyzer
- automatic Ghidra-assisted adapter generation
- universal Addon Engine
- automatic new-level/new-character APIs for arbitrary games

## 🎯 Immediate next milestone

The next major practical milestone is:

```text
Compile V0.4
      ↓
Test SolarLauncher.wps on real Wii U
      ↓
Validate file mods + Patch Engine
      ↓
Extract/analyze Cuphead Wii U RPX
      ↓
Build first Solar Game Adapter
      ↓
Cuphead 3 Player test mod
```

The Cuphead project will therefore be the first serious test of Solar's transition from **universal mod loader** to **game-specific gameplay framework**.

---

# ☀️ Current Direction

Solar Launcher aims to support:

- 🎨 Texture packs
- 📁 File replacement
- 🎵 Custom music and sounds
- ⚙️ Gameplay patches
- 🧠 Memory patches
- 🪝 Function hooks
- 🧩 Multiple mods at the same time
- ⚠️ Mod conflict detection
- 📦 SDCafiine-style packs
- 🎮 Game-specific adapters/APIs
- 🗺️ Custom levels and maps
- 👤 Custom characters
- ➕ Advanced addons
- 🔍 Future automatic RPX analysis and signature scanning

---

# 🎮 General Launch Flow

```text
Wii U Menu
     ↓
Game launched
     ↓
☀ Solar Launcher
     ↓
Detect Title ID
     ↓
Scan compatible mods
     ↓
Load saved selections
     ↓
Solar pre-launch menu
     ↓
Apply selected systems
     ├── File redirection
     ├── Memory patches
     └── Registered Game Adapter hooks
     ↓
Game continues
```

If no compatible mod is installed, the intended behavior is for the game to continue normally.

---

# 🧩 Mod Types

## 📁 1. Replacement Mods

Replacement mods usually do not require RPX reverse engineering or a Game Adapter.

They can replace resources such as:

- textures
- sprites
- music
- sound effects
- UI files
- other game files

Example:

```text
Original:
/vol/content/player/texture.dds

Solar replacement:
SD:/wiiu/SolarLauncher/games/TITLE_ID/MyMod/content/player/texture.dds
```

---

## ⚙️ 2. Patch Mods

Patch mods change values or executable behavior.

A Solar V0.4 memory patch can look like:

```json
{
  "formatVersion": 1,
  "titleId": "00050000XXXXXXXX",
  "patches": [
    {
      "name": "Example value",
      "address": "0x12345678",
      "expected": "00 00 00 02",
      "replace": "00 00 00 03"
    }
  ]
}
```

`expected` is a safety check. Solar should only write the replacement when the target memory already contains the expected bytes.

This helps protect against:

- wrong addresses
- unsupported game versions
- stale patches
- accidental corruption

---

## 🪝 3. Native Hook Mods

Some gameplay changes require actual function hooks instead of simple byte replacement.

Examples:

- creating another player
- changing camera logic
- extending the HUD
- intercepting game functions
- modifying AI or game-state logic

A patch manifest may request a known hook ID:

```json
{
  "hooks": [
    "cuphead.player.createPlayer3",
    "cuphead.camera.multiplayer3"
  ]
}
```

Solar does **not** execute arbitrary native machine code from a mod folder.

The requested hook must already be registered by a trusted **Solar Game Adapter** or built-in adapter module.

---

## ➕ 4. Addons

The long-term goal is to support real additional content instead of only replacements.

Conceptually:

```text
Original Levels
      +
New Fan-Made Level
      +
New Boss
      +
New Character
```

This usually requires a Game Adapter because every game stores, registers and loads its content differently.

---

## 📦 5. Total Mods

A Total Mod combines several Solar systems.

Example:

```text
Cuphead 3 Player

TOTAL MOD
├── File Replacement
│   └── Player 3 sprites / HUD assets
├── Memory Patches
│   └── small executable/data changes
└── Cuphead Game Adapter
    ├── Create Player 3
    ├── Controller 3
    ├── Camera
    ├── HUD
    ├── Revive
    └── Boss targeting
```

---

# 📂 SD Structure

```text
SD:/wiiu/SolarLauncher/
├── games/
│   └── TITLE_ID/
│       ├── ModName/
│       │   ├── mod.json
│       │   ├── content/
│       │   ├── aoc/
│       │   ├── patches/
│       │   └── addons/
│       └── AnotherMod/
│           ├── mod.json
│           ├── content/
│           ├── patches/
│           └── addons/
├── adapters/        # planned external adapter system
├── config/
├── cache/
└── logs/
```

Example `mod.json`:

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

---

# ☕ SDCafiine Compatibility

Solar is designed to understand both native Solar mods and existing SDCafiine-style replacement packs.

### Solar native mod

```text
SD:/wiiu/SolarLauncher/games/TITLE_ID/MyMod/
├── mod.json
└── content/
```

### Existing SDCafiine pack

```text
SD:/wiiu/sdcafiine/TITLE_ID/MyTexturePack/
└── content/
```

For safety during development, running the standalone SDCafiine plugin and Solar file redirection at the same time is not recommended because both may create ContentRedirection layers for the same title.

---

# ⚠️ Mod Conflicts and Priorities

If two enabled mods replace the same virtual path, Solar can detect the conflict.

```text
Base Game
   ↓
Priority 0   — HD Texture Pack
   ↓
Priority 50  — Custom Music
   ↓
Priority 100 — Custom Character
```

For replacement layers, the higher-priority Solar mod is intended to win.

---

# 🌍 What is a Solar Game Adapter?

A **Game Adapter** teaches Solar how a specific game's internal systems work.

The universal Solar Core knows generic operations:

```text
Replace a file
Apply bytes to an address
Enable a mod
Manage priorities
```

But it cannot universally know what an arbitrary function inside a game means.

For example:

```text
CreatePlayer()
RevivePlayer()
UpdateCamera()
RegisterLevel()
```

Every game is different.

A Game Adapter provides that game-specific knowledge.

```text
☀ Solar Launcher
│
├── Universal Core
│   ├── Title Manager
│   ├── Mod Manager
│   ├── Redirect Engine
│   ├── Patch Engine
│   └── Conflict Manager
└── Game Adapters
    ├── Cuphead Adapter
    ├── Mario Kart 8 Adapter
    ├── Minecraft Adapter
    └── Community adapters
```

---

## Example: Cuphead Adapter

A future Cuphead Adapter could expose systems such as:

```text
Cuphead Adapter
├── CreatePlayer3()
├── RegisterController3()
├── ExtendHUD()
├── PatchCameraFor3Players()
├── EnablePlayer3Revive()
└── AddPlayer3BossTargeting()
```

The adapter would contain the addresses, signatures and/or native hook implementations needed for the supported Cuphead version.

---

# ❓ Does every user need to extract their RPX?

**No.**

A normal player using an already-supported mod should ideally only need to:

```text
Download mod
     ↓
Put it on the SD card
     ↓
Solar detects it
     ↓
Launch
```

RPX/RPL analysis is primarily needed when a developer is adding **new deep gameplay support for a game Solar does not understand yet**.

```text
Unsupported game
       ↓
Analyze RPX/RPL once
       ↓
Find important functions/data
       ↓
Create Game Adapter
       ↓
Publish adapter
       ↓
Other users reuse it
```

The goal is for reverse engineering to be done **once by developers/community researchers**, not once by every user.

---

# 🧠 Why advanced mods often need RPX analysis

```text
Texture pack          → usually no RPX analysis
Music replacement     → usually no RPX analysis
UI replacement        → usually no RPX analysis
SDCafiine-style pack  → usually no RPX analysis

Add another player    → likely RPX analysis
Change game logic     → likely RPX analysis
Extend HUD logic      → likely RPX analysis
New entity systems    → likely RPX/RPL analysis
Deep addon support    → likely RPX/RPL analysis
```

The executable must be understood before safe hooks or advanced patches can be created.

---

# 🔍 Future: Solar Auto Analyzer

One long-term goal is to automate much of the repetitive work required to support a new game.

The proposed **Solar Auto Analyzer** would not magically understand an entire game. It would assist reverse engineering.

```text
Game RPX
   ↓
Solar Auto Analyzer
   ↓
Parse executable structure
   ↓
Disassemble PowerPC code
   ↓
Find strings / imports / exports
   ↓
Build references
   ↓
Search known signatures
   ↓
Generate candidate functions
   ↓
Developer verifies results
   ↓
Game Adapter
```

Potential automated tasks:

- identify RPX sections
- locate executable/data regions
- list imports and exports
- extract useful strings
- find references to strings
- locate repeated code patterns
- calculate function signatures
- compare versions of the same game
- search known signatures in another version
- generate adapter templates

For example, strings such as:

```text
Player
PlayerTwo
Join
Camera
Revive
```

could be used as clues to find code worth inspecting.

This reduces the search space but does **not** prove what a function does.

---

# 🧬 Signature Scanning

Hardcoded addresses are fragile because updates may move functions.

```text
Game version A
Player function → 0x02012340

Game version B
Player function → 0x02018A20
```

A future adapter could instead use a validated signature such as:

```text
94 21 ?? ?? 7C 08 02 A6 ?? ?? ?? ??
```

Solar could search for the signature at runtime and resolve the correct address for the current version.

Signatures still require validation to avoid false matches.

---

# 🖥️ Wii U Analyzer vs PC Analyzer

A complete reverse-engineering suite is too heavy for a Wii U plugin, so the proposed design separates the work.

## On Wii U

Future lightweight analysis could include:

- Title ID/version checks
- memory validation
- signature scanning
- resolving already-known functions
- safe patch application

## On PC

A future Solar Analyzer could perform deeper work:

- parse RPX/RPL files
- disassemble PowerPC code
- build cross-references
- inspect strings and functions
- compare versions
- integrate with tools such as Ghidra
- generate adapter skeletons

Possible generated data:

```text
Cuphead.adapter.json
```

or source templates such as:

```cpp
RegisterFunction("cuphead.playerManager", address);
RegisterFunction("cuphead.camera", address);
RegisterFunction("cuphead.revive", address);
```

Human verification remains required before an adapter should be published.

---

# 🚫 Limits of Automatic Reverse Engineering

Automatic analysis can disassemble code, detect patterns and build references, but it cannot safely guarantee from arbitrary code that:

```text
"this function creates Player 3"
```

or:

```text
"this function controls multiplayer revives"
```

without enough evidence.

Solar's intended philosophy is:

> **Automate repetitive reverse-engineering work, not remove the need for validation.**

---

# 🔌 Future External Adapter System

Current V0.4 native hooks are registered inside the compiled Solar plugin.

That is useful for early development, but it is **not** the desired final architecture.

Long term:

```text
SolarLauncher.wps

SD:/wiiu/SolarLauncher/
├── adapters/
│   ├── Cuphead/
│   ├── MarioKart8/
│   └── AnotherGame/
└── games/
```

The goal is to improve Solar Core once so that new game support can be developed independently.

```text
Improve Solar Core
       ↓
Publish adapter interface
       ↓
Develop adapters independently
       ↓
No core redesign for every game
```

The exact secure external native-adapter format is **not implemented yet**.

---

# 🎯 First Advanced Test Project: Cuphead 3 Player

The first planned advanced Game Adapter test is a **3-player mod for the Wii U port of Cuphead**.

Concept:

```text
Player 1 → Cuphead
Player 2 → Mugman
Player 3 → Mugman-based custom character
```

Player 3 is planned to use Mugman's general animation base with:

- a violet/purple nose
- a violet/purple straw
- a distinct grimace during idle

The project should exercise:

- custom player assets
- additional controller handling
- player creation hooks
- camera changes
- HUD extension
- revive behavior
- boss targeting
- memory patches
- file replacement
- Game Adapter integration

The actual Cuphead Adapter work will start after the Wii U Cuphead RPX has been legally extracted and analyzed.

---

# 🌍 Future Game APIs

Once a Game Adapter reliably exposes game systems, Solar could provide higher-level APIs such as:

```text
RegisterLevel()
RegisterCharacter()
RegisterMap()
RegisterBoss()
RegisterMusic()
RegisterWeapon()
RegisterItem()
```

The available API would depend entirely on the target game and the adapter implementation.

---

# 🔧 Solar Architecture

```text
☀ Solar Launcher
│
├── Title Manager
│   ├── current game
│   ├── Title ID
│   └── version awareness
├── Mod Manager
│   ├── scan mods
│   ├── read mod.json
│   ├── enable / disable
│   └── priorities
├── Redirect Engine
│   ├── content
│   ├── AOC
│   └── SDCafiine compatibility
├── Patch Engine
│   ├── memory patches
│   ├── expected-byte validation
│   ├── restoration
│   └── hook requests
├── Conflict Manager
│   └── replacement conflicts
├── Native Hook Registry
│   └── registered adapter hooks
├── Game Adapter Layer
│   └── future game-specific integrations
├── Auto Analyzer (future)
│   ├── RPX/RPL analysis
│   ├── signatures
│   ├── references
│   └── adapter assistance
└── Addon Engine (future)
    ├── levels
    ├── maps
    ├── characters
    └── additional content
```

---

# 🗓️ Roadmap

## v0.1 — Solar Core

Implemented in development code.

## v0.2 — File Mods

Implemented in development code.

## v0.3 — Mod Management

Implemented in development code.

## v0.4 — Patch Engine

Current development milestone.

Main focus now: **compile + real-console validation**.

## v0.5 — Game Adapter Foundation

Planned:

- formal Game Adapter interface
- signature resolver
- version-aware adapter data
- Cuphead Adapter research
- first Cuphead 3-player experiments

## v0.6 — Cuphead Multiplayer Expansion

Planned:

- stable 3-player gameplay
- optional 4-player expansion
- P3/P4 controllers
- HUD extension
- camera changes
- revive support
- boss targeting
- custom character assets

## Future

- external Game Adapter format
- Solar Auto Analyzer
- PC RPX/RPL Analyzer
- Ghidra-assisted adapter generation
- signature database
- advanced addons
- custom levels/maps/characters
- mod profiles
- community-created Game APIs

---

# 🤝 Contributions

Solar Launcher is intended to become an open Wii U modding framework.

Useful contributions include code, testing, documentation, RPX/RPL research, signature research, adapter development, mods, bug reports, UI work, tools and tutorials.

The long-term goal is that research done for one game can be packaged into an adapter and reused by the rest of the community.

---

# ❤️ Credits

## ☀️ Project Origins & Development

**Solar Launcher** was created thanks to the work of **Pixel Plugins Studios**, the development and project direction of **Eitan1414**, and development assistance from **OpenAI's GPT-5.6 Sol**.

- **Pixel Plugins Studios** — work and contributions that helped make the creation of Solar Launcher possible.
- **Eitan1414** — creator/developer, project direction, implementation, testing, design and development of Solar Launcher.
- **OpenAI's GPT-5.6 Sol** — development assistance, technical research, brainstorming, architecture design, code assistance and support throughout the creation of Solar Launcher.

The name **Solar Launcher** is a reference to **GPT-5.6 Sol**, as a small tribute for its help across this project and other Wii U development projects.

> Solar Launcher is an independent community project and is not officially affiliated with or endorsed by OpenAI.

## 🛠️ Wii U Homebrew Community

Special thanks to the developers and contributors behind:

- Aroma
- Wii U Plugin System (WUPS)
- wut
- devkitPro / devkitPPC
- ContentRedirectionModule
- FunctionPatcherModule
- SDCafiine
- FTPiiU Everywhere

## 🎮 Cuphead Wii U

Special thanks to **The Latte Team** for their work on the Wii U port of **Cuphead**, planned as one of Solar Launcher's first advanced modding test cases.

The Cuphead multiplayer project is a separate community modification.

**Cuphead**, its characters, artwork and related intellectual property belong to **Studio MDHR** and their respective rights holders.

---

# ⚠️ Disclaimer

Solar Launcher is an unofficial homebrew project.

It is not affiliated with or endorsed by Nintendo, OpenAI, Studio MDHR, The Latte Team, or any game publisher/developer unless explicitly stated otherwise.

Users should provide their own legally obtained games and game files.

Solar Launcher does not aim to distribute copyrighted game assets or game executables.

Game names and trademarks belong to their respective owners.

---

<p align="center">
  ☀️ <b>Solar Launcher</b><br>
  <i>Universal Wii U modding framework</i>
</p>

<p align="center">
  <b>Built for the Wii U modding community.</b>
</p>