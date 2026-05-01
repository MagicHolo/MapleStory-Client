# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HeavenClient is a custom, open-source MapleStory v83 game client written in C++ for Windows. Compatible with HeavenMS servers (v229.2). All code lives in namespace `ms`.

## Build Commands

**Build from command line** (run from project root):
```powershell
# Auto-detect MSBuild via vswhere
& (& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\amd64\MSBuild.exe") MapleStory.vcxproj /p:Configuration=Release /p:Platform=x64 /m /nologo
```

Common configurations: `Release|x64` (primary), `Debug|x64` (console output), `Release|Win32`, `Debug|Win32`.

**Build in Visual Studio**: Open `MapleStory.vcxproj`, then Ctrl+Shift+B.

**Clean build**: Clean Solution, delete `.vs/`, `x64/`, `x86/`, `debug.log`, `MapleStory.aps`, `Settings`, then rebuild.

**Run the client**: `x64\Release\MapleStory.exe` — needs `.nx` data files in the same directory. Connects to `127.0.0.1:8484` by default (configurable via `Settings` file, `ServerIP` entry).

**Run the server**: Requires [HeavenMS](https://github.com/ryantpayton/MapleStory) with Java 8 and MySQL/MariaDB. See `CLAUDE.local.md` for machine-specific paths.

## Build System Details

- **Project file**: MapleStory.vcxproj (Visual Studio / MSBuild)
- **Platforms**: Win32 (x86), x64
- **Configurations**: Debug, Release
- **Forced include**: `MapleStory.h` is included in every `.cpp` via vcxproj — defines feature toggles and logging macros globally
- **Post-build**: Automatically copies `freetype.dll`, `bass.dll`, `liblz4.dll` to output directory

**Feature toggles** (in MapleStory.h):
- `USE_NX` — Use NX format for game data (default: on)
- `USE_CRYPTO` — AES encryption for server communication (default: on)
- `USE_ASIO` — Use Asio networking instead of Winsock (default: off)
- `LOG_LEVEL` — `LOG_DEBUG` in Debug, `LOG_WARN` in Release

## NX Data Pipeline

Game assets come from MapleStory's `.wz` files, converted to `.nx` format using [NoLifeWzToNx](https://github.com/ryantpayton/NoLifeWzToNx). The client reads NX files via the NoLifeNx library (memory-mapped, read-only).

**Required NX files** (11, defined in `Util/NxFiles.h`):
Character.nx, Etc.nx, Item.nx, Map.nx, Mob.nx, Npc.nx, Quest.nx, Reactor.nx, Skill.nx, String.nx, UI.nx

**v83 vs newer data**: This client was originally designed for post-Big Bang WZ data. v83 WZ data has different node paths — particularly `Login.img/Title` (not `Title_new`), `Back/login.img` (not `UI_login.img`), and no split files (Map001.nx, Sound001.nx etc. — `nx.cpp` falls back to the main file). Base.nx and Effect.nx may not exist in v83 — a stub Base.nx (valid PKG4 header, 106 bytes) is needed. Sound.nx and TamingMob.nx are optional.

**NX node access pattern**:
```cpp
nl::node data = nl::nx::UI["Login.img"]["Title"]["BtLogin"];
```

Global NX file handles are declared in `includes/NoLifeNx/nlnx/nx.hpp` and loaded by `nl::nx::load_all()` in `includes/NoLifeNx/nlnx/nx.cpp`. The prebuilt x86 `.lib` ships in the repo; the x64 `.lib` was manually compiled from the NoLifeNx source files + `lz4.c` with `/O2 /MD /EHsc /std:c++17`.

## Architecture

```
MapleStory.cpp (entry, main loop: fixed 8ms timestep + interpolated render)
    |
    v
Core Singletons (accessed via ClassName::get(), inherit Singleton<T>):
  Session, Stage, Window, GraphicsGL, UI, Configuration, Camera, Timer, Sound
    |
    v
Subsystems:
  Graphics/    -- OpenGL rendering, textures, animations, text (FreeType)
  IO/          -- Window, keyboard, UI element management
  Gameplay/    -- Stage, physics (footholds), combat, map objects
  Character/   -- Player, OtherChar, Mob, Npc, inventory, skills, look
  Net/         -- Session, packets, handlers, crypto
  Audio/       -- Sound/Music playback (Bass library)
  Data/        -- Game data queries from NX files
  Template/    -- Generic types: Optional<T>, Cache<K,V>, EnumMap, BoolPair
```

### Main Loop

Init order: Session -> NxFiles -> Window -> Sound/Music -> Char -> DamageNumber -> MapPortals -> Stage -> UI

Game loop: accumulate elapsed time, call `update()` in fixed 8ms steps, then `draw(alpha)` with interpolation for smooth rendering.

### UI System

`UIElement` is the base class. ~80 concrete types in `IO/UITypes/` (UILogin, UICharSelect, UIStatusBar, UIInventory, UISkillBook, etc.). Three mutually exclusive states: LOGIN, GAME, CASHSHOP — each a `UIState` subclass managing its UI elements.

Create: `UI::get().emplace<UIType>(args...)`. Retrieve: `UI::get().get_element<UIType>()`.

Buttons use `MapleButton` (4-state texture: normal/pressed/mouseOver/disabled) or `TwoSpriteButton`. Textfields handle keyboard input with callbacks.

### Networking

Packets flow: Server -> Socket -> Session::read() -> PacketSwitch::forward(opcode) -> PacketHandler subclass -> game state updated. ~500 opcodes routed via array lookup. Handlers in `Net/Handlers/`, packet definitions in `Net/Packets/`.

### Rendering

Origin (0,0) at top-left. DrawArgument encapsulates position/scale/rotation/opacity. Textures drawn at `position - origin` (origin is the anchor point within the bitmap). Background layers from map data may need horizontal/vertical tiling based on their `type` property.

## Dependencies (all vendored in includes/)

| Library | Purpose |
|---------|---------|
| GLFW 3.3.2 | Window/input |
| GLEW 2.1.0 | OpenGL extensions |
| FreeType | Font rendering |
| Bass 2.4 | Audio |
| NoLifeNx | NX file reading |
| LZ4 1.8.2 | Decompression (used by NoLifeNx) |
| STB | Image/font utilities |

## Code Style

- Classes: `PascalCase`, functions: `snake_case`, constants: `UPPER_SNAKE_CASE`
- No member prefixes (`m_`, `_`, etc.)
- All code in `namespace ms`
- Singletons accessed via `::get()` static method
- Error pattern: `if (Error error = init()) { ... }` with retryable error codes

## References

- **Server**: https://github.com/ryantpayton/MapleStory (HeavenMS)
- **WZ->NX converter**: https://github.com/ryantpayton/NoLifeWzToNx
- **NX library**: https://github.com/ryantpayton/NoLifeNx
- **Linux port**: https://github.com/ryantpayton/HeavenClient/tree/linux
