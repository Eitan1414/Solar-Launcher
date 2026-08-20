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

1. **Universal modding** — file replacement, SDCafiine-style packs, mod selection, priorities, conflict handling and declarative memory patches.
2. **Game-specific modding** — advanced gameplay modifications powered by trusted Game Adapters.

> ⚠️ Solar Launcher is still experimental. V0.1–V0.4 foundations are implemented in development code and V0.5 development has started, but the current V0.5 branch has **not yet been fully compiled and validated on a real Wii U**.

---

# 🚧 Current Project Status

The project is currently at **Solar Launcher V0.5 — Game Adapter Foundation**.

Current development branch:

```text
solar-gameadapter-v0.5
```

Solar now contains a real WUPS/Aroma codebase with universal mod loading plus the beginning of game-specific runtime support.

The first advanced target is **Cuphead Wii U**.

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
- registered hook IDs for trusted adapter code

### V0.5 — Game Adapter Foundation

Implemented so far on the V0.5 branch:

- `GameAdapterRegistry`
- first built-in `CupheadAdapter`
- reusable `MonoBridge` foundation for Unity/Mono games
- FunctionPatcher hook registration for `mono_compile_method`
- runtime tracing of selected managed gameplay methods
- validation of known Mono metadata helper signatures before calling them
- Cuphead-specific metadata isolated inside the Cuphead Adapter instead of the generic Mono Bridge
- first Cuphead diagnostic mod requesting `cuphead.mono.compileTrace`
- first branded Solar pre-launch interface pass

The current Cuphead Adapter is intentionally a **research/diagnostic adapter**, not yet a finished multiplayer adapter.

---

# ☕ Current Cuphead Research

A legally extracted Wii U Cuphead installation has now provided the files needed for the first adapter research pass.

Verified target information:

```text
Title ID:      0005000021000000
Title version: 0
Executable:    Unity-master.rpx
Runtime:       Unity / Mono
Gameplay DLL:  Assembly-CSharp.dll
```

The extracted `app.xml` and `cos.xml` confirm the target Title ID/version and `Unity-master.rpx` executable.

The supplied `Managed` directory also contains the game's managed gameplay assemblies, allowing Solar development to inspect classes involved in multiplayer behavior.

Relevant managed systems identified during research include names such as:

```text
PlayerManager
PlayerInput
AbstractPlayerController
PlayerCameraController
Level
LevelHUD
LevelHUDPlayer
CreatePlayerTwoOnJoin
SetupPlayerTwo
RevivePlayer
PlayerSuperGhost
```

The current research indicates that many surrounding systems explicitly assume two players, so a stable 3-player implementation will require more than changing a single player-count value.

An experimental third player ID is currently planned as:

```text
Player 1 = 0
Player 2 = 1
Player 3 = 2
```

This does **not** mean Player 3 is already functional. It is the ID reserved for the next experiments.

---

# 🧪 Cuphead 3 Player — Test 1

The first V0.5 Cuphead test does **not** create Player 3 yet.

Its purpose is to obtain real runtime method/code addresses from a real Wii U before invasive gameplay hooks are attempted.

The test mod requests:

```text
cuphead.mono.compileTrace
```

Flow:

```text
Cuphead starts
      ↓
Solar detects 0005000021000000
      ↓
Cuphead Adapter is prepared
      ↓
Test mod requests cuphead.mono.compileTrace
      ↓
Solar hooks mono_compile_method
      ↓
Selected managed methods are compiled
      ↓
Solar logs their managed + native code pointers
```

Current traced class targets include:

```text
PlayerManager
PlayerInput
AbstractPlayerController
PlayerCameraController
Level
LevelHUD
LevelHUDPlayer
```

Expected log output will be written to:

```text
SD:/wiiu/SolarLauncher/logs/solar.log
```

with lines conceptually similar to:

```text
Mono Trace: PlayerManager::... method=0xXXXXXXXX code=0xXXXXXXXX
Mono Trace: PlayerInput::... method=0xXXXXXXXX code=0xXXXXXXXX
Mono Trace: Level::... method=0xXXXXXXXX code=0xXXXXXXXX
```

Those logs will be used to design the first real P3 runtime experiment.

See [`docs/V0.5_GAME_ADAPTER.md`](docs/V0.5_GAME_ADAPTER.md) for the current V0.5 adapter notes.

---

# 🎯 Immediate Next Milestone

The current practical sequence is:

```text
Compile Solar V0.5
      ↓
Obtain SolarLauncher.wps
      ↓
Test V0.5 on a real Wii U under Aroma
      ↓
Validate the new Solar interface
      ↓
Run Cuphead Mono Trace Test 1
      ↓
Collect solar.log
      ↓
Confirm runtime player-system addresses
      ↓
Build Cuphead P3 Test 2
      ↓
Attempt PlayerId 2 / third-player creation
```

After creation of P3 works, the remaining systems will be added incrementally:

```text
P3 creation
   ↓
Controller 3
   ↓
Camera
   ↓
HUD
   ↓
Revive / ghost behavior
   ↓
Boss targeting
   ↓
P3 assets / visual identity
```

---

# 🎨 Solar V0.5 Interface

V0.5 introduces the first branded Solar Launcher pre-launch interface.

The goal is to keep a Wii U homebrew / SDCafiine-inspired identity while remaining lightweight.

## Binary / ASCII Solar logo

The interface recreates the Solar sun primarily with:

```text
0 1 / \\ | - . '
```

The `0` and `1` characters provide the binary aesthetic while the extra ASCII characters help preserve the silhouette and central lightning/L shape of the original Solar logo.

The logo assembles line-by-line during a short startup animation.

## Current layout

```text
       binary Solar sun           SOLAR LAUNCHER
                                  Universal Wii U modding framework
                                  Load. Combine. Expand.

---------------------------------------------------------------------
 MODS                               MOD INFORMATION

 > [ON ] Cuphead 3 Player Test      Name: ...
   [OFF] Another Mod                Author: ...
   [ON ] SDCafiine Pack             Version: ...
                                    Type: ...
                                    Priority: ...
                                    Conflicts: ...
                                    Payload: C:yes A:no P:yes
                                    Source: Solar / SDCafiine
---------------------------------------------------------------------
 A Toggle   X Details   L/R Priority   Y Reset
 + Save & launch mods   B Launch vanilla once
 0101 SOLAR READY 1010
```

Current controls:

- D-Pad Up/Down — select a mod
- A — enable/disable
- X — switch between normal information and technical details
- L/R — change priority
- Y — reset selected mod to defaults
- Plus — save and launch selected mods
- B — launch vanilla once

The current renderer uses Wii U `OSScreen`:

- black background
- white built-in monospace text
- Solar-orange pixel separators/chrome

The built-in OSScreen font is white-only, so fully coloured text would require a richer future renderer such as a GX2-based UI.

See [`docs/V0.5_INTERFACE.md`](docs/V0.5_INTERFACE.md) for the interface design notes.

---

# 🧪 Still Needs Validation

Solar should **not** yet be treated as a stable release.

The following still needs real-console validation:

- successful production compilation of the current `SolarLauncher.wps`
- plugin boot under Aroma
- V0.5 binary/ASCII interface on TV
- V0.5 binary/ASCII interface on GamePad
- controller navigation in the new layout
- multiple simultaneous replacement mods
- SDCafiine compatibility
- conflict and priority behavior
- memory patch application/restoration
- invalid-address / wrong-expected-byte safety
- FunctionPatcher lifecycle
- Cuphead `mono_compile_method` hook
- Mono metadata signature validation on real hardware
- `Mono Trace:` log generation
- long-session stability

A V0.5 `.wps` has **not yet been validated on real Wii U hardware**, so screenshots, behavior and runtime logs from the console remain an essential next step.

---

# ⏳ Not Implemented Yet

The following are still planned rather than finished features:

- actual Cuphead Player 3 creation
- Cuphead Controller 3 support
- 3-player camera behavior
- third HUD panel
- P3 revive / ghost support
- P3 boss targeting
- stable Cuphead 3-player gameplay
- Cuphead 4-player gameplay
- external Game Adapters loaded independently from Solar Core
- automatic signature resolver
- Solar Auto Analyzer
- PC RPX/RPL Analyzer
- automatic Ghidra-assisted adapter generation
- universal Addon Engine
- automatic new-level/new-character APIs for arbitrary games

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
- 🧬 Unity/Mono runtime research through adapters where appropriate
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
Prepare matching built-in Game Adapter (if supported)
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

Patch mods can change values or request trusted runtime behavior.

A declarative memory patch can look like:

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

This helps protect against wrong addresses, unsupported versions, stale patches and accidental corruption.

---

## 🪝 3. Native Hook Mods

Some gameplay changes require function hooks instead of simple byte replacement.

A patch manifest may request a known hook ID:

```json
{
  "hooks": [
    "cuphead.mono.compileTrace"
  ]
}
```

Solar does **not** execute arbitrary native machine code from a mod folder.

The requested hook must already be registered by a trusted Solar Game Adapter or built-in adapter module.

The first real V0.5 example is the Cuphead Mono Trace hook.

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

A Total Mod can combine several Solar systems.

Future Cuphead 3 Player concept:

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
│           ├── aoc/
│           ├── patches/
│           └── addons/
├── adapters/        # planned external adapter system; not active yet
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

Cuphead Test 1 path:

```text
SD:/wiiu/SolarLauncher/games/0005000021000000/Cuphead3PlayerTest/
├── mod.json
└── patches/
    └── mono-trace.patch.json
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

# 🌍 What Is a Solar Game Adapter?

A **Game Adapter** teaches Solar how a specific game's internal systems work.

The universal Solar Core knows generic operations:

```text
Replace a file
Apply validated bytes to an address
Enable a mod
Manage priorities
Request a registered hook
```

But it cannot universally know what arbitrary game-specific functions mean.

For example:

```text
CreatePlayer()
RevivePlayer()
UpdateCamera()
RegisterLevel()
```

Every game is different.

A Game Adapter provides that game-specific knowledge.

Current V0.5 architecture direction:

```text
☀ Solar Launcher
│
├── Universal Core
│   ├── Title Manager
│   ├── Mod Manager
│   ├── Redirect Engine
│   ├── Patch Engine
│   └── Conflict Manager
├── Native Hook Registry
├── Game Adapter Registry
├── Mono Bridge
└── Built-in Game Adapters
    └── Cuphead Adapter
```

The Cuphead Adapter is the **first real adapter foundation** in the repository.

It does not yet expose a finished `CreatePlayer3()` API. Its current role is to identify the supported title/runtime and register the Mono compile-trace hook needed for research.

---

# ❓ Does Every User Need to Extract Their RPX?

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

RPX/RPL and managed-code analysis is primarily needed when developers add **new deep gameplay support for a game Solar does not understand yet**.

```text
Unsupported game
       ↓
Analyze RPX/RPL / runtime once
       ↓
Find important functions/data
       ↓
Create Game Adapter
       ↓
Publish reusable support
       ↓
Other users reuse it
```

The Cuphead RPX/Managed extraction was needed for **development of the adapter**, not as a planned requirement for every future user of a finished Cuphead mod.

---

# 🧠 Why Advanced Mods Often Need RPX Analysis

```text
Texture pack          → usually no RPX analysis
Music replacement     → usually no RPX analysis
UI replacement        → usually no RPX analysis
SDCafiine-style pack  → usually no RPX analysis

Add another player    → likely RPX/runtime analysis
Change game logic     → likely RPX/runtime analysis
Extend HUD logic      → likely RPX/runtime analysis
New entity systems    → likely RPX/RPL/runtime analysis
Deep addon support    → likely game-specific analysis
```

The executable/runtime must be understood before safe hooks or advanced patches can be created.

---

# 🧬 Mono Bridge

V0.5 introduces an experimental **Mono Bridge** foundation for supported Unity/Mono games.

Its first use is Cuphead research.

The key idea is:

```text
Game starts
   ↓
Mono compiles a managed method
   ↓
Solar observes mono_compile_method
   ↓
Adapter identifies interesting managed classes/methods
   ↓
Solar records the resulting native code pointer
```

The Mono Bridge itself is intended to remain reusable.

Game-specific addresses, validation signatures and class target lists belong in the corresponding adapter profile rather than being hardcoded into the generic bridge.

This system is experimental and still requires real Wii U validation.

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

Solar's intended philosophy is:

> **Automate repetitive reverse-engineering work, not remove the need for validation.**

---

# 🧬 Future Signature Scanning

Hardcoded addresses are fragile because updates may move functions.

A future adapter can use validated signatures/patterns to resolve targets more safely across supported versions.

Signatures still require validation to avoid false matches.

---

# 🖥️ Wii U Analyzer vs PC Analyzer

A complete reverse-engineering suite is too heavy for a Wii U plugin, so the proposed design separates the work.

## On Wii U

Future lightweight analysis may include:

- Title ID/version checks
- memory validation
- signature scanning
- resolving already-known functions
- safe patch application
- lightweight runtime tracing for supported engines

## On PC

A future Solar Analyzer could perform deeper work:

- parse RPX/RPL files
- disassemble PowerPC code
- build cross-references
- inspect strings and functions
- compare versions
- integrate with tools such as Ghidra
- generate adapter skeletons

Human verification remains required before an adapter should be published.

---

# 🔌 Future External Adapter System

The current V0.5 Cuphead Adapter is **built into the compiled Solar plugin**.

That is useful for early development, but it is not the desired final architecture.

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

The exact secure external native-adapter format is **not implemented yet**.

The goal is to improve Solar Core once, publish a stable adapter interface, and allow game support to evolve independently without redesigning Solar for every title.

---

# 🎯 First Advanced Project: Cuphead 3 Player

The first advanced Game Adapter project is a **3-player mod for the Wii U port of Cuphead**.

Concept:

```text
Player 1 → Cuphead
Player 2 → Mugman
Player 3 → Mugman-based custom character
```

Planned Player 3 visual identity:

- Mugman-based general style/proportions
- violet/purple nose
- violet/purple straw
- distinct idle grimace

The project is intended to exercise:

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
- Mono/runtime research

Current state: **runtime research / Test 1**, not playable 3-player gameplay yet.

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

The available API would depend entirely on the target game and adapter implementation.

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
├── Game Adapter Registry
│   └── select built-in adapter for current title
├── Mono Bridge
│   └── reusable Unity/Mono runtime tracing foundation
├── Built-in Game Adapters
│   └── Cuphead Adapter (V0.5 research stage)
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

Implemented in development code; still needs full compile/console validation in the current development flow.

## v0.5 — Game Adapter Foundation

**Current development milestone.**

Implemented/started:

- Game Adapter Registry
- built-in Cuphead Adapter foundation
- reusable Mono Bridge
- Cuphead Mono compile trace hook
- Cuphead runtime research profile
- first Mono Trace test mod
- branded V0.5 Solar interface

Still required for V0.5:

- successful `.wps` build
- real Wii U validation
- retrieve Test 1 logs
- map runtime player systems
- first Player 3 creation experiment
- early version/signature resolver work

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

**Solar Launcher is currently the main project of Pixel Plugins Studios.**

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

Special thanks to **The Latte Team** for their work on the Wii U port of **Cuphead**, which is being used as Solar Launcher's first advanced Game Adapter research target.

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
