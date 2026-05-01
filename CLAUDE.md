# CLAUDE.md

## Project Overview

HeavenClient — open-source MapleStory v83 client in C++ (Windows). Compatible with [HeavenMS](https://github.com/ryantpayton/MapleStory) v229.2. All code in `namespace ms`.

## Build

```powershell
# Auto-detect MSBuild and build (run from project root)
& (& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\amd64\MSBuild.exe") MapleStory.vcxproj /p:Configuration=Release /p:Platform=x64 /m /nologo
```

Configurations: `Release|x64` (primary), `Debug|x64` (has console), `Release|Win32`, `Debug|Win32`.

**Run**: `x64\Release\MapleStory.exe` — requires `.nx` data files in same directory. Connects to `127.0.0.1:8484` (configurable in `Settings` → `ServerIP`).

**Clean**: delete `.vs/`, `x64/`, `x86/`, `debug.log`, `MapleStory.aps`, `Settings`, rebuild.

**Server setup**: see `CLAUDE.local.md` for machine-specific paths.

## Build System

- `MapleStory.vcxproj` — MSBuild project, Win32/x64, Debug/Release
- `MapleStory.h` force-included in every `.cpp` — feature toggles + logging macros
- Post-build copies `freetype.dll`, `bass.dll`, `liblz4.dll` to output

**Feature toggles** (MapleStory.h): `USE_NX` (on), `USE_CRYPTO` (on), `USE_ASIO` (off), `LOG_LEVEL` (DEBUG/WARN by config).

## NX Data

Game assets: `.wz` → `.nx` via [NoLifeWzToNx](https://github.com/ryantpayton/NoLifeWzToNx). Read by NoLifeNx (memory-mapped).

**Required** (11, in `Util/NxFiles.h`): Character, Etc, Item, Map, Mob, Npc, Quest, Reactor, Skill, String, UI.

**v83 differences**: `Login.img/Title` (not `Title_new`), `Back/login.img` (not `UI_login.img`), no split files (fallback in `nx.cpp`). Base.nx needs a stub (106-byte PKG4 header). Sound.nx/TamingMob.nx optional.

**Access pattern**: `nl::node data = nl::nx::UI["Login.img"]["Title"]["BtLogin"];`

NX handles in `includes/NoLifeNx/nlnx/nx.hpp`, loaded by `nl::nx::load_all()`. x86 `.lib` prebuilt; x64 `.lib` compiled from source + `lz4.c` (`/O2 /MD /EHsc /std:c++17`).

## Architecture

```
MapleStory.cpp (entry — fixed 8ms timestep + interpolated render)
  Core Singletons (::get(), inherit Singleton<T>):
    Session, Stage, Window, GraphicsGL, UI, Configuration, Camera, Timer, Sound
  Subsystems:
    Graphics/   — OpenGL, textures, animations, text (FreeType)
    IO/         — Window, keyboard, UI elements
    Gameplay/   — Stage, physics, combat, map objects
    Character/  — Player, OtherChar, Mob, Npc, inventory, skills
    Net/        — Session, packets, handlers, crypto
    Audio/      — Sound/Music (Bass)
    Data/       — NX data queries
    Template/   — Optional<T>, Cache<K,V>, EnumMap, BoolPair
```

**Init**: Session → NxFiles → Window → Sound → Char → DamageNumber → MapPortals → Stage → UI

**UI**: `UIElement` base, ~80 types in `IO/UITypes/`. States: LOGIN, GAME, CASHSHOP (`UIState` subclasses). Create via `UI::get().emplace<T>()`, retrieve via `UI::get().get_element<T>()`. Buttons: `MapleButton` (4-state) or `TwoSpriteButton`.

**Net**: Server → Socket → `Session::read()` → `PacketSwitch::forward(opcode)` → `PacketHandler` → state update. ~500 opcodes, array lookup. Handlers in `Net/Handlers/`.

**Render**: Origin top-left. `DrawArgument` = position/scale/rotation/opacity. Drawn at `position - origin`.

## Dependencies (vendored in includes/)

GLFW 3.3.2, GLEW 2.1.0, FreeType, Bass 2.4, NoLifeNx, LZ4 1.8.2, STB.

## Code Style

`PascalCase` classes, `snake_case` functions, `UPPER_SNAKE_CASE` constants. No member prefixes. All in `namespace ms`. Singletons via `::get()`. Errors: `if (Error error = init()) { ... }`.

## References

[HeavenMS](https://github.com/ryantpayton/MapleStory) · [WzToNx](https://github.com/ryantpayton/NoLifeWzToNx) · [NoLifeNx](https://github.com/ryantpayton/NoLifeNx) · [Linux port](https://github.com/ryantpayton/HeavenClient/tree/linux)
