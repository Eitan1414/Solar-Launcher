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

The goal is to provide one common system for several kinds of Wii U mods instead of requiring a completely different workflow for every project.

Solar detects the title being launched through its **Title ID**, scans compatible mods on the SD card, lets the user enable or disable them, and then applies the supported modifications before the game continues.

Solar is being designed around two levels of modding:

1. **Universal modding**, which can work for many games without understanding their executable code.
2. **Game-specific modding**, which uses optional Game Adapters for advanced gameplay modifications.

> ⚠️ Solar Launcher is still experimental. V0.1–V0.4 are active development branches and must still be validated extensively on real Wii U hardware.

---

## ☀️ Current Direction

Solar Launcher aims to support:

- 🎨 Texture packs
- 📁 File replacement
- 🎵 Custom music and sounds
- ⚙️ Gameplay patches
- 🧠 Memory patches
- 🪝 Function hooks
- 🧩 Multiple mods at the same time
- ⚠️ Mod conflict detection
- 📦 SDCafiine-style mod packs
- 🎮 Game-specific adapters/APIs
- 🗺️ Custom levels and maps
- 👤 Custom characters
- ➕ Advanced addons that add new content instead of only replacing existing files
- 🔍 Future automatic RPX analysis and signature scanning

---

## ✅ What Solar V0.4 is designed to do

Solar V0.4 builds on the previous versions.

### Solar Core

- detect the currently launched Wii U game using its Title ID
- create and scan Solar directories on the SD card
- read `mod.json`
- keep per-game mod selections
- log Solar activity

### File Mods

- redirect `/vol/content`
- redirect `/vol/aoc`
- load multiple replacement packs
- use priorities between replacement layers
- detect existing SDCafiine-style packs

### Mod Management

- pre-launch mod menu
- enable or disable mods
- change mod priority
- save selections per Title ID
- detect replacement-file conflicts
- launch once without mods

### Patch Engine

- scan `patches/`
- apply declarative memory patches
- verify expected bytes before writing
- reject unsafe/mismatching patches
- restore original bytes when the game closes
- prepare native function hooks through FunctionPatcherModule
- provide a registry for future Game Adapter hooks

> The code exists in the V0.4 branch, but the final `.wps` still needs full compile and real-console validation before Solar can be considered stable.

---

## 🎮 General Launch Flow

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
     └── Game Adapter hooks
     ↓
Game continues
```

If no compatible mod is installed, the game should launch normally.

---

# 🧩 Mod Types

## 📁 1. Replacement Mods

Replacement mods do not normally need a Game Adapter or reverse engineering of the RPX.

They can replace things such as:

- textures
- sprites
- music
- sound effects
- UI files
- other game resources

Example:

```text
Original game file:
/vol/content/player/texture.dds

        ↓

Solar replacement:
SD:/wiiu/SolarLauncher/games/TITLE_ID/MyMod/content/player/texture.dds
```

These mods use the same general concept as SDCafiine while integrating into Solar's own mod manager.

---

## ⚙️ 2. Patch Mods

Patch mods change values or executable behavior.

Examples:

- changing a gameplay value
- changing a player limit
- changing timers
- modifying game rules
- changing instructions in memory

A Solar V0.4 memory patch can use a format like:

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

The `expected` bytes are important.

Solar should only apply the patch when the bytes currently in memory match the expected bytes. This reduces the risk of writing a patch to the wrong game version or wrong address.

---

## 🪝 3. Native Hook Mods

Some gameplay modifications cannot be implemented with a simple value replacement.

For example:

- creating a third player
- changing camera logic
- extending a HUD
- adding a new game state
- modifying AI behavior
- intercepting a game function

For these cases Solar uses **native hooks**.

A patch manifest can request a hook by ID:

```json
{
  "hooks": [
    "cuphead.player.createPlayer3",
    "cuphead.camera.multiplayer3"
  ]
}
```

However, Solar does **not** execute arbitrary native machine code from the SD card.

The requested hook ID must already be registered by a trusted **Solar Game Adapter** compiled for that game.

---

## ➕ 4. Addons

The long-term goal is to support real additional content.

Instead of only doing:

```text
Original Level
      ↓
Modified Level
```

an addon could eventually allow:

```text
Original Levels
      +
New Fan-Made Level
      +
New Boss
      +
New Character
```

This kind of integration usually requires a Game Adapter because every game stores and registers content differently.

---

## 📦 5. Total Mods

A Total Mod combines several Solar systems.

Example:

```text
Cuphead 3 Player

TOTAL MOD
├── File Replacement
│   └── Player 3 sprites / HUD assets
│
├── Memory Patches
│   └── Small executable/data changes
│
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
│       │
│       └── AnotherMod/
│           ├── mod.json
│           ├── content/
│           ├── patches/
│           └── addons/
│
├── adapters/        # planned external adapter system
├── config/
├── cache/
└── logs/
```

A basic mod can look like:

```text
MyMod/
├── mod.json
├── content/
└── patches/
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

This means users should not need to rebuild an old texture pack just because they start using Solar.

For safety, running the standalone SDCafiine plugin and Solar's file redirection at the same time is not recommended while Solar is in development, because both can create ContentRedirection layers for the same game.

---

# ⚠️ Mod Conflicts and Priorities

If two enabled mods replace the same path, Solar can detect that conflict.

Example:

```text
HD Texture Pack
└── player/character.texture

Custom Character
└── player/character.texture
```

Solar can report that both mods modify the same file.

The higher-priority Solar layer should win.

```text
Base Game
   ↓
Priority 0   — HD Texture Pack
   ↓
Priority 50  — Custom Music
   ↓
Priority 100 — Custom Character
```

---

# 🌍 What is a Solar Game Adapter?

A **Game Adapter** teaches Solar how a specific game's internal systems work.

Solar's universal systems know how to do generic operations such as:

```text
Replace a file
Apply bytes to an address
Enable a mod
Manage priorities
```

But Solar cannot universally know what a game's functions mean.

For example, Solar cannot automatically assume that a random function inside a game is:

```text
CreatePlayer()
RevivePlayer()
UpdateCamera()
RegisterLevel()
```

Every game is built differently.

A Game Adapter provides that missing game-specific knowledge.

```text
☀ Solar Launcher
│
├── Universal Core
│   ├── Title Manager
│   ├── Mod Manager
│   ├── Redirect Engine
│   ├── Patch Engine
│   └── Conflict Manager
│
└── Game Adapters
    ├── Cuphead Adapter
    ├── Mario Kart 8 Adapter
    ├── Minecraft Adapter
    └── Other community adapters
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

A mod could then request those known hooks without needing to contain executable code itself.

The Cuphead Adapter would contain the game-specific function addresses, signatures or hook implementations that Solar needs.

---

# ❓ Does every user need to extract their RPX?

**No.**

There are two different roles:

### Normal user

A normal user who downloads an already-supported mod should not need to extract or reverse engineer anything.

```text
Download mod
     ↓
Put it on the SD card
     ↓
Solar detects it
     ↓
Launch
```

### Developer adding support for a new game

If a developer wants to create a deep gameplay mod for a game Solar does not understand yet, that developer may need to analyze that game's RPX and sometimes related RPL files.

```text
New unsupported game
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

The reverse engineering should therefore be done **once by the developer/community**, not once by every player.

---

# 🧠 Why advanced mods often need RPX analysis

Simple replacement mods usually do not require the executable.

```text
Texture pack             → usually no RPX analysis
Music replacement        → usually no RPX analysis
UI replacement           → usually no RPX analysis
SDCafiine-style pack     → usually no RPX analysis
```

Advanced gameplay mods are different.

```text
Add another player       → likely RPX analysis
Change game logic        → likely RPX analysis
Extend HUD logic         → likely RPX analysis
Add new entity systems   → likely RPX analysis
Deep addon integration   → likely RPX/RPL analysis
```

The RPX is useful because it contains the executable code of the game that must be understood before safe hooks or patches can be created.

---

# 🔍 Future: Solar Auto Analyzer

One of Solar's long-term goals is to reduce the amount of manual reverse engineering required when supporting a new game.

The proposed **Solar Auto Analyzer** would not magically understand an entire game, but it could automate a large amount of repetitive analysis.

Concept:

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
Build references between code and data
   ↓
Search known signatures
   ↓
Generate candidate functions
   ↓
Developer verifies results
   ↓
Game Adapter
```

---

## What the analyzer could realistically automate

Potential automated tasks include:

- identify RPX sections
- locate executable and data regions
- list imports and exports
- extract useful strings
- find references to strings
- locate repeated code patterns
- calculate function signatures
- compare different versions of the same game
- search a known signature in another region/version
- identify likely functions for manual inspection
- generate adapter templates

For example, if analysis finds strings such as:

```text
Player
PlayerTwo
Join
Camera
Revive
```

Solar Analyzer could identify the code that references those strings and mark those functions as candidates for further investigation.

This does **not** prove what a function does, but it can greatly reduce the search space.

---

# 🧬 Signature Scanning

Hardcoded addresses are fragile because a game update can move a function.

Example:

```text
Cuphead version A
Player function → 0x02012340

Cuphead version B
Player function → 0x02018A20
```

Instead of storing only one address, an adapter can eventually use a signature:

```text
94 21 ?? ?? 7C 08 02 A6 ?? ?? ?? ??
```

Solar can search for the signature when the game launches and resolve the current address automatically.

This can make one adapter more tolerant of different versions, although signatures still need validation to prevent false matches.

---

# 🖥️ Wii U Analyzer vs PC Analyzer

A full reverse-engineering suite would be too heavy and unnecessary inside a Wii U plugin.

The proposed design separates the work.

## On the Wii U

Solar can eventually perform lightweight runtime analysis:

- Title ID detection
- version checks
- memory validation
- signature scanning
- resolving known functions
- safe patch application

## On PC

A future **Solar Analyzer** desktop tool could perform deeper offline analysis:

- parse RPX/RPL files
- disassemble PowerPC code
- build cross-references
- analyze strings and functions
- compare versions
- integrate with tools such as Ghidra
- generate adapter skeletons

Possible output:

```text
Cuphead.adapter.json
```

or generated source such as:

```cpp
RegisterFunction("cuphead.playerManager", address);
RegisterFunction("cuphead.camera", address);
RegisterFunction("cuphead.revive", address);
```

A developer would then review and test those results before publishing the adapter.

---

# 🚫 What automatic analysis cannot guarantee

Automatic reverse engineering has limits.

A machine can disassemble code and detect patterns, but it cannot safely guarantee from arbitrary code that:

```text
"this function creates Player 3"
```

or:

```text
"this function controls multiplayer revives"
```

without enough symbols, recognizable patterns, previous knowledge or human validation.

Solar's goal should therefore be:

> **Automate the repetitive reverse-engineering work, not remove the need for validation.**

A realistic workflow is:

```text
RPX
 ↓
Automatic analysis
 ↓
Candidate functions / signatures
 ↓
Human verification
 ↓
Adapter
 ↓
Reusable support for the community
```

---

# 🔌 Future External Adapter System

The long-term architecture should avoid recompiling Solar Launcher every time support for another game is added.

Current V0.4 native hooks are registered inside the compiled Solar plugin. This is useful for development, but it is not the desired final architecture.

The planned direction is:

```text
SolarLauncher.wps

SD:/wiiu/SolarLauncher/
├── adapters/
│   ├── Cuphead/
│   ├── MarioKart8/
│   └── AnotherGame/
│
└── games/
```

A community developer could then create a Game Adapter separately from the Solar Core.

The goal is:

```text
Improve Solar once to support adapters
              ↓
Develop new game adapters independently
              ↓
No need to redesign the core for every game
```

The exact secure module format for external native adapters is **not implemented yet** and must be designed carefully before arbitrary adapter loading is enabled.

---

# 🎯 First Advanced Test Project: Cuphead 3 Player

One of Solar's first advanced test cases is planned to be a **3-player mod for the Wii U port of Cuphead**.

The current concept is:

```text
Player 1 → Cuphead
Player 2 → Mugman
Player 3 → Mugman-based custom character
```

Player 3 is planned to use Mugman's general animation base with:

- a violet/purple nose
- a violet/purple straw
- a distinct grimace during idle

The project should test several Solar systems together:

- custom player assets
- additional controller handling
- player creation hooks
- camera modifications
- HUD extension
- revive behavior
- boss targeting
- memory patches
- file replacement
- Game Adapter integration

The actual Cuphead Adapter work will begin after the Wii U Cuphead RPX has been legally extracted and analyzed.

---

# 🌍 Future Game APIs

Once a Game Adapter exposes reliable internal systems, Solar could provide a higher-level API for mod creators.

Conceptually:

```text
RegisterLevel()
RegisterCharacter()
RegisterMap()
RegisterBoss()
RegisterMusic()
RegisterWeapon()
RegisterItem()
```

The available API would depend entirely on what the target game supports and what the adapter has implemented.

---

# 🗺️ Example Future Cuphead Addon

A future Cuphead addon might look like:

```text
FanmadeIsland/
├── addon.json
├── island.json
├── levels/
│   ├── boss01/
│   ├── boss02/
│   └── runngun01/
├── map/
│   └── island5/
├── sprites/
├── music/
└── sounds/
```

Solar would detect the addon and the Cuphead Adapter/API would handle the game-specific integration.

---

# 🔧 Solar Architecture

```text
☀ Solar Launcher
│
├── Title Manager
│   ├── Detect current game
│   ├── Read Title ID
│   └── Version awareness
│
├── Mod Manager
│   ├── Scan mods
│   ├── Read mod.json
│   ├── Enable / disable
│   └── Priorities
│
├── Redirect Engine
│   ├── Content files
│   ├── AOC files
│   └── SDCafiine compatibility
│
├── Patch Engine
│   ├── Memory patches
│   ├── Expected-byte validation
│   ├── Patch restoration
│   └── Native hook requests
│
├── Conflict Manager
│   └── Replacement conflicts
│
├── Native Hook Registry
│   └── Registered Game Adapter hooks
│
├── Game Adapter Layer
│   ├── Cuphead
│   ├── Mario Kart 8
│   └── Community adapters
│
├── Auto Analyzer (future)
│   ├── RPX parsing
│   ├── signatures
│   ├── references
│   └── adapter generation assistance
│
└── Addon Engine (future)
    ├── Levels
    ├── Maps
    ├── Characters
    └── Additional content
```

---

# 🗓️ Development Roadmap

## v0.1 — Solar Core

- Title ID detection
- SD mod scanning
- `mod.json`
- basic configuration
- logging

## v0.2 — File Mods

- ContentRedirection integration
- `/vol/content` replacement layers
- `/vol/aoc` replacement layers
- multiple file mods
- SDCafiine pack detection

## v0.3 — Mod Management

- pre-launch menu
- enable / disable mods
- per-game selections
- priorities
- file conflict detection
- vanilla launch option

## v0.4 — Patch Engine

- declarative memory patches
- expected-byte validation
- original-byte restoration
- overlapping-patch protection
- FunctionPatcher integration
- Native Hook Registry foundation
- patch-only mod support

## v0.5 — Game Adapter Foundation

Planned:

- formal Game Adapter interface
- Cuphead Adapter research
- first Cuphead 3-player experiments
- signature resolver
- version-aware adapter data

## v0.6 — Cuphead Multiplayer Expansion

Planned:

- stable 3-player gameplay
- optional 4-player expansion
- P3/P4 controllers
- HUD extensions
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
- custom levels
- custom maps
- mod profiles
- community-created Game APIs

---

# 🤝 Contributions

Solar Launcher is intended to become an open Wii U modding framework.

Useful contributions include:

- code
- testing
- documentation
- game research
- RPX/RPL reverse engineering
- signature research
- adapter development
- mod development
- bug reports
- UI design
- tools
- tutorials

The long-term goal is that research done for one game can be packaged into an adapter and reused by the rest of the community.

---

# ❤️ Credits

## ☀️ Project

**Solar Launcher**

Created and led by **Eitan1414**.

Concept, project direction, testing, design and original idea by the Solar Launcher project creator.

---

## 🤖 Development Assistance

Special thanks to **OpenAI's GPT-5.6 Sol** for development assistance, technical research, brainstorming, architecture design and support throughout the creation of Solar Launcher.

The name **Solar Launcher** is a reference to **GPT-5.6 Sol**, as a small tribute for its help across this project and other Wii U development projects.

> Solar Launcher is an independent community project and is not officially affiliated with or endorsed by OpenAI.

---

## 🛠️ Wii U Homebrew Community

Solar Launcher builds upon years of work from the Wii U homebrew community.

Special thanks to the developers and contributors behind projects and tools such as:

- **Aroma**
- **Wii U Plugin System (WUPS)**
- **wut**
- **devkitPro**
- **devkitPPC**
- **ContentRedirectionModule**
- **FunctionPatcherModule**
- **SDCafiine**
- **FTPiiU Everywhere**

Their work makes projects like Solar Launcher possible.

---

## 🎮 Cuphead Wii U

Special thanks to **The Latte Team** for their work on the Wii U port of **Cuphead**, planned as one of Solar Launcher's first advanced modding test cases.

The Cuphead multiplayer mod is intended as a separate community modification.

**Cuphead**, its characters, artwork and related intellectual property belong to **Studio MDHR** and their respective rights holders.

---

# ⚠️ Disclaimer

Solar Launcher is an unofficial homebrew project.

It is not affiliated with or endorsed by:

- Nintendo
- OpenAI
- Studio MDHR
- The Latte Team
- any game publisher or developer unless explicitly stated otherwise

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
