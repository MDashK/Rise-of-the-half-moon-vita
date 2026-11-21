# Rise of the Half Moon (offline for PC & PS Vita)

The 8 Google Doodle **Rise of the Half Moon** games (October 2024 to May 2025), playable without an internet connection:

- **PC (Windows)** — C# / WPF app that runs the original HTML5 games in WebView2, served from a local copy.
- **PS Vita** — native version in C++20 + SDL2: the rules, AI, and screens have been reimplemented from the
  original source code; the graphics, sounds, video, and text are the original assets, downloaded by the app itself.

Port by MDashK / AI-assisted by Claude.

> The games (graphics, music, sounds, video, and text) are © Google LLC and are **not included** in this repository
> or in the releases: both apps download them from Google's servers on first launch.
> Unofficial, non-commercial project for personal use and preservation.

## Releases

| Platform | File | Installation |
|---|---|---|
| Windows 10/11 (x64) | `RiseHalfMoon-PC-v<version>-win-x64.zip` | Extract `RiseHalfMoon.exe` to a writable folder (not `Program Files`) and run it |
| PS Vita (HENkaku/Ensō) | `RiseHalfMoon-PSVita-v<version>.vpk` | Install using VitaShell |

On first launch, click **Download / update games** (an internet connection is only required this once).

| Game | Original folder (`https://www.google.com/logos/2024/moon/…`) |
|---|---|
| October 2024 | `r3/` |
| November 2024 | `novr2/` |
| December 2024 | `december-r4/` |
| January 2025 | `moon_january-r1/` |
| February 2025 | `moon_february-r4/` |
| March 2025 | `moon_march-rc2/` |
| April 2025 | `moon_april-rc2/` |
| May 2025 | `moon_may-rc3/` |

---

## PC (Windows)

### Usage

- **Language**: Portuguese (PT and BR), English, Spanish, French, German, Italian, and Dutch. Selecting a language
  from the menu immediately changes both the interface and the language of the games. On first launch, the app uses
  the Windows language if it is one of the supported languages.
- **?** opens the *About* dialog (version, credits, copyright, and licenses).
- Click a game to play. The game toolbar contains **Menu**, **Restart**, and **Fullscreen (F11)**.
- **Download / update games** saves everything to the `Jogos/` folder next to the `.exe`
  (files that are already there are not downloaded again).

### How it works

- `Jogos/` stores an exact copy of every required file, organized by URL
  (`Jogos/www.google.com/logos/2024/moon/...`, fonts in `Jogos/fonts.gstatic.com/...`, etc.).
- Each game opens in WebView2 using its original address, but the app **intercepts all requests** and serves the files
  from `Jogos/`. No requests are sent out to the internet:
  - Google telemetry (`gen_204`, `async/ddllog`) receives an empty response;
  - any other address is blocked (external links, such as sharing links, open in the default browser).
- Scores, progress, and the selected language are stored in `%LOCALAPPDATA%\RiseHalfMoonOffline`.

### Command-line arguments

| Argument | Effect |
|---|---|
| `--data <folder>` | Uses a different games folder (defaults to `Jogos` next to the `.exe`) |
| `--download` | Downloads everything without opening a window, then exits (log written to `download.log`) |
| `--offline` | Never uses the internet, even to retrieve missing files |
| `--play <1-8>` | Immediately opens the specified game |

### Build

Requirements: .NET 8 SDK and Microsoft Edge WebView2 Runtime (already included with Windows 11 and most Windows 10 installations).

Open `RiseHalfMoon.sln` in Visual Studio 2022 and press F5, or run:

```bash
dotnet run --project src/RiseHalfMoon
```

Release build — a single `RiseHalfMoon.exe` (~70 MB), which does not require .NET to be installed. The games are not bundled; the app downloads them:

```bash
dotnet publish src/RiseHalfMoon -c Release -r win-x64 --self-contained true -p:IncludeGames=false -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true -p:DebugType=none -o publish
```

If a `Jogos/` folder exists in the repository root, development builds copy it next to the
`.exe` (useful for testing without downloading the games).

---

# PS Vita

Native version (C++20 + SDL2) of the "Rise of the Half Moon" games. The original games are HTML5 and
cannot run on the Vita, so the rules, AI, and screens have been reimplemented from the original source code.
The graphics, sounds, video, and text are the original assets, downloaded by the app itself.

- VPK: `build-vita/RiseHalfMoon.vpk` (Title ID `RHMOON001`)
- User data: `ux0:/data/rise_half_moon/` (games in `games/`, `settings.json`, `save_<month>.json`)
- Touch-only interface with large buttons. Landscape menu; the game can be played in landscape or portrait
  orientation (portrait matches the mobile version), with an option to rotate 180°.
- Menu and games available in 8 languages (the same as on PC); changing the language in the menu updates everything immediately.
- **?** button: *About* dialog with the version (automatically increments with each build), credits, and licenses.

## Status

All 8 months are playable: intro video, tutorial, campaign, AI, and each month's 4 wildcards.

| Month | Wildcards |
|---|---|
| October 2024 | Hunter Moon, Leonids, Scorpio, Super Moon (legacy engine: finite deck, one card per turn) |
| November 2024 | Winter Solstice, Geminids, Sagittarius, Beaver Moon (legacy engine) |
| December 2024 | Capricorn, Long Night Moon, Light of Mars, Quadrantids |
| January 2025 | Aquarius, Light of Venus, Moon Apogee, Wolf Moon |
| February 2025 | Pisces, Snow Moon, Total Lunar Eclipse, Equinox |
| March 2025 | Aries, Worm Moon, Solar Eclipse, Lyrids |
| April 2025 | Taurus, Pink Moon, Eta Aquarids, Close Approach |
| May 2025 | Gemini, Flower Moon, Moon at Perigee, Hercules Cluster |

### Build (WSL with VitaSDK)

Dependencies (vdpm): `sdl2`, `curl-mbedtls` (installs `mbedtls`), `zlib`, `zstd`.

```bash
cd vita
export VITASDK=/usr/local/vitasdk PATH=/usr/local/vitasdk/bin:$PATH
cmake -S . -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-vita -j8
```

The VPK is generated at `vita/build-vita/RiseHalfMoon.vpk`. More details (extracted data, diagnostics) are available in
[`vita/README.md`](vita/README.md).

## Data extracted from the original games

`tools/extract_month.py` reads the original JavaScript (formatted using jsbeautifier) and generates `assets/data/<month>.json`:
boards and campaign data, sprite frames, animations, sounds, intro text, wildcards, and CSS (colors).
It requires Python 3 with `py_mini_racer`:

```bash
python tools/extract_month.py <game.pretty.js> <month> assets/data/<month>.json <folder-containing-py_mini_racer>
```

Supports both formats used by the original game (October/November and December through May).

## Diagnostics

Create a `ux0:/data/rise_half_moon/debug.txt` file containing any of the following keywords to enable the corresponding options:

| Keyword | Effect |
|---|---|
| `trace` | Displays the latest game events on screen and writes them to `trace.txt` |
| `autoplay` | The player is also controlled by the AI (for testing entire campaigns) |
| `fast` | 4x faster game speed |
| `allwild` | All of the month's wildcards are unlocked from the start |
| `game=<month>` | Immediately opens the specified game (`oct`, `nov`, `dec`, `jan`, `feb`, `mar`, `apr`, `may`) |
| `reveal` | Shows the new-wildcard animation when entering the campaign |
| `nv21` | Uses the legacy video color order (for Vita3K only, which ignores U/V order) |

---

## Credits and licenses

- "Rise of the Half Moon" © Google LLC. Google and Google Doodle are trademarks of Google LLC.
- PC: Microsoft Edge WebView2 SDK (BSD-3-Clause), .NET / WPF (MIT).
- PS Vita: SDL2 (zlib), libcurl (curl), Mbed TLS (Apache 2.0), zlib, Zstandard (BSD), stb (public domain),
  Mozilla CA certificates (MPL 2.0), Josefin Sans font (SIL OFL 1.1), VitaSDK.
- PlayStation and PS Vita are trademarks of Sony Interactive Entertainment Inc. Microsoft, Windows, and Microsoft Edge
  are trademarks of the Microsoft group of companies. This project is not affiliated with or endorsed by these companies.
