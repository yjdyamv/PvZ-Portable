# PvZ-Portable

<div align="center">
  <img src="icon-readme.png" alt="PvZ-Portable" width="450">
</div>

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/wszqkzqk/PvZ-Portable)

A **cross-platform** community-driven reimplementation of Plants vs. Zombies: Game of the Year Edition, aiming to bring the **100% authentic experience** of Plants vs. Zombies to every platform.

| 🌿 Authentic | 🎮 Portable | 🛠️ Open |
| :---: | :---: | :---: |
| Almost 100% gameplay recreation | Support for 32/64 bit systems<br>Run on Linux, Windows, macOS, Switch... | OpenGL ES 2.0 & SDL |

**⚠️ Notice:**

* This repository does **NOT** contain any copyrighted game assets (such as images, music, or fonts) owned by PopCap Games or Electronic Arts. Users must provide their own `main.pak` and `properties/` folder from a **legally purchased copy** of Plants vs. Zombies: GOTY Edition.
* The codebase is a manual reimplementation derived from publicly available reverse-engineering documentation and community research (such as [植物大战僵尸吧](https://tieba.baidu.com/f?ie=utf-8&kw=%E6%A4%8D%E7%89%A9%E5%A4%A7%E6%88%98%E5%83%B5%E5%B0%B8), [PVZ Wiki](https://wiki.pvz1.com/doku.php?id=home) and [PvZ Tools](https://pvz.tools/memory/)). It is written to utilize portable backends like SDL2 and OpenGL ES 2.0 (with desktop OpenGL 2.1 fallback).
* This project is intended solely for **educational purposes**, focusing on **cross-platform porting techniques**, engine modernization, and learning how classic game logic can be adapted to various hardware architectures (e.g., Nintendo Switch, 3DS).
* Non-Commercial: This project is not affiliated with, authorized, or endorsed by PopCap Games or Electronic Arts.
* Project icons and platform-specific logos are created by me (wszqkzqk) with the help of AI image generation tools and are not official assets of PopCap/EA.
* To play the game using this project you **MUST** have access to the original game files by purchasing it on [EA's official website](https://www.ea.com/games/plants-vs-zombies/plants-vs-zombies) or [Steam](https://store.steampowered.com/app/3590/Plants_vs_Zombies_GOTY_Edition/).

## Features

This project is **based on** [Patoke](https://github.com/Patoke/re-plants-vs-zombies) and [Headshotnoby](https://github.com/headshot2017/re-plants-vs-zombies)'s PvZ GOTY implementation with the following objectives:
- [x] Replace renderer with SDL + OpenGL ES 2.0 (desktop OpenGL 2.1 fallback)
  - Also enable to **resize the window**, which was not possible in the original game
  - **Why OpenGL ES 2.0?** GLES 2.0 is the common subset of virtually all modern GPU APIs — every desktop OpenGL 2.1+ driver, mobile GPU, and game console inherently supports it. This means the game works **out of the box** everywhere without extra dependencies. [ANGLE](https://chromium.googlesource.com/angle/angle) can also be optionally used to translate calls to DirectX/Metal/Vulkan if needed.
- [x] Replace Windows code with cross-platform code
- [x] Replace DirectSound/BASS/FMod with [SDL Mixer X](https://github.com/WohlSoft/SDL-Mixer-X)
  - This project uses a fork of SDL Mixer X that adds compatibility with the MO3 format by using libopenmpt. This fork is located under SexyAppFramework/sound/SDL-Mixer-X
- [x] main.pak support
- [x] Optimize memory usage for console ports (Partial)
- [x] **Compatible** with original PvZ GOTY Edition's ***global user data*** (profile info, adventure progress, coins, Zen Garden, etc., stored in `user*.dat`)
  - [x] Fix 2038 year problem while keeping compatibility
- [x] **Portable mid-level save game** format `.v4` support (share **mid-level saves** between Windows, Linux, macOS, Switch, etc.)
  - [x] Support export/import of `.v4` save files to/from human-readable YAML format for easy editing
- [x] Implement with `std::thread` for cross-platform threading support
- [x] Implement with `std::filesystem` for cross-platform file system support
- [x] Replace wide-string with `std::string` and UTF-8 encoding
- [x] 32 and 64-bit builds support
- [x] Different CPU architectures support (i686, x86_64, aarch64, riscv64, loongarch64, etc.)
- [x] Unicode path support on all platforms
- [x] Different endianness support (little-endian and big-endian)
  - [x] Save data compatibility across endianness
  - Theoretically supports big-endian platforms, but untested due to lack of hardware

This project supports the following platforms:

| Platform        | Data path                    | Status                                                                                 |
|-----------------|------------------------------|----------------------------------------------------------------------------------------|
| Linux (SDL2)    | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| Windows (SDL2)  | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| macOS (SDL2)    | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| Haiku (SDL2)    | Executable dir (resources); per-user app-data for writable files | Partially works: no music                                                              |
| Nintendo Switch | sdmc:/switch/PvZPortable | Works on real hardware. Kenji-NX crashes on boot.                           |
| Nintendo 3DS    | sdmc:/3ds/PvZPortable    | In development, might not have enough memory for Old 3DS, might barely work on New 3DS |

To play the game, you need the game data from PvZ GOTY. Place `main.pak` and the `properties/` folder next to the `pvz-portable` executable (the game will search for resources relative to the executable's directory). You can also use extracted data instead of `main.pak` if you prefer.

Note about writable data and caches:

- The game will read resources (like `main.pak` and `properties/`) from the executable directory by default, so you can launch the binary from any working directory and it will still find them.
- Per-user writable files (settings, savegames, compiled caches, screenshots) are stored in the **OS-recommended application data path**. With the current build these are under `io.github.wszqkzqk/PvZPortable` and include subfolders such as:
  - `userdata/` — Player save files.
  - `cache64/` if you use the 64-bit version or `cache32/` if you use the 32-bit version — Compiled binary caches (reanimation / compiled definitions). These caches are **local startup** artifacts (**native layout**), not portable files; when cache/schema checks fail, the game transparently recompiles from source data.
  - `registry.regemu` — Settings/registry emulation.

Examples:

- Linux: `~/.local/share/io.github.wszqkzqk/PvZPortable/`
- Windows: `%APPDATA%\io.github.wszqkzqk\PvZPortable\`
- macOS: `~/Library/Application Support/io.github.wszqkzqk/PvZPortable/`

You can customize these paths via command-line parameters:
- `-resdir="<path>"`: Set the **resource directory** (where `main.pak` and `properties/` are located). This only affects where the game looks for resources, not where it saves data.
- `-savedir="<path>"`: Set the **save data directory** (where settings, savegames, caches, and screenshots are stored). This overrides the default OS-recommended application data path.

**Note:** You **MUST** use the format `-param="<Your Path>"`. Space-separated values (e.g. `-resdir path`) are **NOT** supported.

## Game Version Compatibility

This project is designed and tested against Plants vs. Zombies **GOTY Edition 1.2.0.1073** (the standalone PopCap release). Using the **Steam GOTY Edition 1.2.0.1096** assets is largely supported — the gameplay is fully functional — but there are some known cosmetic issues due to breaking changes in the 1.2.0.1096 `LawnStrings.txt` format.

**Note:** The following issues **only occur when using 1.2.0.1096 (Steam) assets**. They **do not affect 1.2.0.1073 users**.

| Issue (1.2.0.1096 only) | Cause |
| :---: | :---: |
| **Almanac blue description text missing** | In 1.2.0.1096, the plain-text introductory paragraph was split out from `[XXX_DESCRIPTION]` into a new `[XXX_DESCRIPTION_HEADER]` key. The engine only reads `[XXX_DESCRIPTION]`, so the header text is never displayed. |
| **"Restart" button label missing** | The key `[RESTART_LEVEL]` (used for the button label) was renamed to `[RESTART_LEVEL_BUTTON]` in 1.2.0.1096. |
| **Unencountered zombie shows `???`** instead of `(not encountered yet)` | The string `[NOT_ENCOUNTERED_YET]` changed its value to `???` in 1.2.0.1096; the old text was moved to a new key `[NOT_ENCOUNTERED_YET_DESCRIPTION]`. |
| **Crazy Dave's plant sell price shows 1/10 of the correct value** | In 1.2.0.1073, the string template `[CRAZY_DAVE_1700]` contains a trailing `0` after `{SELL_PRICE}` (i.e. `${SELL_PRICE}0`) because the engine passes the price divided by 10. In 1.2.0.1096 the trailing `0` was removed, so the displayed price becomes 1/10 of the intended amount. |

**Recommendation: use the 1.2.0.1073 asset package whenever possible.** The 1.2.0.1096 (Steam) version works as a fallback, but the issues listed above will affect the in-game UI.

## Dependencies

Before building on PC, ensure you have the necessary dependencies installed:

- **Build Tools**: `CMake`, `Ninja`, A C/C++ compiler (e.g., `gcc`, `clang`, `MSVC`)
- **Graphics**: `OpenGL ES 2.0` or `OpenGL 2.1+` (auto-detected at runtime via SDL2)
- **Audio**: `libopenmpt`, `libogg`, `libvorbis`, `mpg123`
- **Image**: `libpng`, `libjpeg-turbo`
- **Windowing/Input**: `SDL2`

### Arch Linux

You can install the required dependencies using the following command:

```bash
sudo pacman -S --needed base-devel cmake libjpeg-turbo libogg libopenmpt libpng libvorbis mpg123 ninja sdl2-compat
```

### Debian/Ubuntu

You can install the required dependencies using the following command:

```bash
sudo apt install cmake ninja-build libogg-dev libjpeg-dev libopenmpt-dev libpng-dev libvorbis-dev libmpg123-dev libsdl2-dev
```

### Windows (MSYS2 UCRT64)

You can install the required dependencies using the following command:

```bash
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libjpeg-turbo mingw-w64-ucrt-x86_64-libopenmpt mingw-w64-ucrt-x86_64-libogg mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-libvorbis mingw-w64-ucrt-x86_64-mpg123 mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2
```

### macOS (Homebrew)

You can install the required dependencies using [Homebrew](https://brew.sh/) with the following command:

```bash
brew install cmake dylibbundler jpeg-turbo libogg libopenmpt libpng libvorbis mpg123 ninja sdl2
```

## Build Instructions

Run the following commands (assuming you have CMake and other dependencies installed) where the `CMakeLists.txt` file is located:

```bash
cmake -G Ninja -B build
```

```bash
cmake --build build
```

### Performance Optimization

It is recommended to use the **Release** build type for the best performance, as it usually implies compiler optimizations:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
```

For advanced users, you can also specify `CFLAGS` and `CXXFLAGS` to enable architecture-specific optimizations (e.g. `-march=native` to fully utilize your CPU's instruction set):

```bash
cmake -G Ninja -B build -DCMAKE_C_FLAGS="-march=native" -DCMAKE_CXX_FLAGS="-march=native" -DCMAKE_BUILD_TYPE=Release
```

### Configuration Options

You can customize the game features by adding options to the first `cmake` command:

| Option | Default | Description |
| :--- | :--- | :--- |
| `PVZ_DEBUG` | `OFF`<br>(`ON` if `CMAKE_BUILD_TYPE` is `Debug`) | Enable **cheat keys**, debug displays and other debug features. |
| `LIMBO_PAGE` | `ON` | Enable access to the limbo page which contains hidden levels. |
| `DO_FIX_BUGS` | `OFF` | Apply community fixes for "bugs" of official 1.2.0.1073 GOTY Edition.[^1] However, these "bugs" are usually **considered "features"** by many players. |
| `CONSOLE` | `OFF`<br>(`ON` if `CMAKE_BUILD_TYPE` is `Debug`) | Show a console window (Windows only). |

[^1]: Current `DO_FIX_BUGS` includes the following fixes:
    - Fix bungee zombie duplicate sun/item drop in I, Zombie mode.
    - Make mind-controlled Gargantuars smash enemy zombies instead of plants.
    - Make mind-controlled Gargantuars throw mind-controlled Imps (with scale, health, and direction fixes).
    - Make mind-controlled Gargantuars can smash vases in Scary Potter mode.
    - Make mind-controlled Pea/Gatling Head zombies shoot forward instead of backward.
    - Make mind-controlled Jalapeno/Squash zombies damage enemy zombies instead of plants.
    - Coordinate fixes for mind-controlled Squash zombies tracking and smashing enemy zombies.
    - Make mind-controlled Jalapeno zombies correctly clear Dr. Zomboss' skills (Iceball/Fireball) and ladder logic.
    - Sync Dancer Zombie animations (fixes "Maid" displacement bug).
    - Fix visual glitch of Ladder Zombie's arm recovery.
    - Fix Dr. Zomboss' attack (RV, Fireball/Iceball) and summon range coverage for 6-lane (Pool) levels.

Example: Manually enable `PVZ_DEBUG` in **Release build** so that you can use **cheat keys** while having optimized performance:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DPVZ_DEBUG=ON
```

If running these commands does not create a successful build please create an issue and detail your problem.

## Save data compatibility (user data and mid-level saves)

PvZ-Portable uses two distinct types of save data:

1.  **Global User Data** (`users.dat`, `user1.dat`, etc.):
    *   Stores profile info, adventure progress, coins, Zen Garden, etc.
    *   **Fully compatible** with the original PC game format.
    *   Already portable by design (uses explicit serialization).

2.  **Mid-Level Save States** (`game1_0.v4`, etc. legacy `game1_0.dat`, etc.):
    *   Stores the exact state of a level when "Save and Exit" is used (zombies, projectiles, plants, etc.).
    *   The game now writes **only** `*.v4` files by default:
        *   `*.v4` files: **Portable format**. Sharing these files to transfer progress between different platforms is fully **supported**.
        *   `*.dat` files: **Legacy dump** from old versions. Contains raw memory dumps. **Do not share this file** across platforms as it will cause crashes due to architecture differences.
    *   When loading, the game **prefers** `.v4`; `*.dat` is fallback-only for migration compatibility.
    *   If a save is loaded from legacy `*.dat`, the game automatically re-saves it to `*.v4` and removes the legacy `*.dat` after successful migration.

### Why a new mid-level save format?

The original game's mid-level save format (`.dat`) effectively dumps raw memory structures to disk. This is fast but fragile, as it relies on specific memory layouts that break across:

*   **Different CPU Architectures**: x86, ARM, RISC-V, LoongArch (alignment).
*   **32-bit vs 64-bit builds**: Pointer sizes and struct layouts differ.
*   **Different Compilers**: MSVC vs GCC/Clang (padding, enum size, struct packing).

As a result, legacy saves are generally not guaranteed to load across those variants. To solve this, **Format v4** serializes game objects field-by-field using Type-Length-Value (TLV) tags. This ensures that a level saved on a x86_64 Linux machine can be loaded on a LoongArch or ARM (Switch) machine without crashing.

### Developer Guidelines for Format v4

- Add new fields as new TLV IDs; do not reuse IDs.
- Keep defaults for missing fields when loading older saves.
- Avoid raw struct dumps for data that may change layout; prefer explicit per-field sync with fixed-width primitives.

### Save Editing & Conversion Tool

There's a Python script `scripts/pvzp-v4-converter.py` to inspect and modify `.v4` save files. 

**Features:**
*   **Info**: View basic save stats (Wave, Sun, Zombies, etc.).
*   **Export to YAML**: Convert binary `.v4` files to human-readable YAML to view/edit gameplay-relevant data.
*   **Import from YAML**: Convert modified YAML back to `.v4` format with correct checksums.

**Usage:**
```bash
# Modify these paths as needed
# View basic info
python scripts/pvzp-v4-converter.py info ~/.local/io.github.wszqkzqk/PvZPortable/userdata/game1_13.v4

# Export to YAML for editing
python scripts/pvzp-v4-converter.py export ~/.local/io.github.wszqkzqk/PvZPortable/userdata/game1_13.v4 level.yaml

# Import back to savegame
# **BACKUP** your original .v4 file fist!
mv ~/.local/io.github.wszqkzqk/PvZPortable/userdata/game1_13.v4 ~/.local/io.github.wszqkzqk/PvZPortable/userdata/game1_13.v4.bak
python scripts/pvzp-v4-converter.py import level.yaml ~/.local/io.github.wszqkzqk/PvZPortable/userdata/game1_13.v4
```

## Contributing

When contributing please follow the following guides:

<details><summary>SexyAppFramework coding philosophy</summary>

### From the SexyAppFramework docs:

<br>
The framework differs from many other APIs in that some class properties are not wrapped in accessor methods, but rather are made to be accessed directly through public member data.   The window caption of your application, for example, is set by assigning a value to the std::string mTitle in the application object before the application’s window is created.  We felt that in many cases this reduced the code required to implement a class.  Also of note is the prefix notation used on variables: “m” denotes a class member, “the” denotes a parameter passed to a method or function, and “a” denotes a local variable.
</br>
</details>

## License

**Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>**

This project is licensed under the terms of the [**GNU Lesser General Public License v3.0**](https://www.gnu.org/licenses/lgpl-3.0.html) or later (LGPL-3.0-or-later).

* The repository includes complete license texts at the root:
  * `LICENSE` — LGPL-3.0 text
  * `COPYING` — GPL-3.0 text, referenced by LGPL-3.0
* The code is provided "as is", **WITHOUT WARRANTY** of any kind.
* The **original game IP (Plants vs. Zombies) belongs to PopCap/EA**. This license applies **only to the code implementation** in this repository.
* This project does **NOT** include any copyrighted assets from the original game.

### PopCap Games Framework Acknowledgment

The `SexyAppFramework` directory may contain code originally based on the **PopCap Games Framework**. This code is subject to the permissive **[PopCap Games Framework License](src/SexyAppFramework/LICENSE)**. To the extent that original code remains, the following acknowledgment applies:

> "This product includes portions of the PopCap Games Framework, © 2005-2009 PopCap Games, Inc.  All rights reserved. (http://popcapframework.sourceforge.net/)."

Note that this code has been **heavily refactored**, **optimized** and **modernized** by the community over time under the **LGPL-3.0-or-later** license.

## Thanks

- **[@Headshotnoby](https://www.github.com/headshot2017)**: For almost fully implementing the 64-bit support and the initial fixed-function OpenGL backend.
- **[@Patoke](https://www.github.com/Patoke)**: For the incredible initial reimplementation of PvZ GOTY.
- **[@rspforhp](https://www.github.com/octokatherine)**: For the 0.9.9 version's work.
- **[@ruslan831](https://github.com/ruslan831)**: For archiving the 0.9.9 version's re-implementation.
- **PopCap Games**: For creating the amazing game and releasing their framework to the public with a permissive license.
- **The SDL Team**: For the amazing cross-platform development library that powers this port.
- **The GLFW Team**: For their work on the windowing library used in the original re-implementation.
- **The OpenMPT Team**: For libopenmpt, enabling high-quality MO3 music playback.
- All the contributors who have worked or are actively working in this amazing project.
