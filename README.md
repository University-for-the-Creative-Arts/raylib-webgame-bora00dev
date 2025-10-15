# WaveBreaker – Web & Desktop Builds

WaveBreaker is a Raylib-powered top‑down shooter that targets both native Windows and the browser (via Emscripten).
This guide covers building, serving, and the runtime config request the game performs at startup.

---

## Requirements
- Raylib sources at `C:\raylib\raylib`
- Raylib w64devkit toolchain (`C:\raylib\w64devkit\bin`)
- Emscripten SDK installed at `C:\Users\bora2\emsdk`

---

## Build (Desktop)
```powershell
Set-Location "C:\Users\bora2\OneDrive\Desktop\GameDev Projects\Projects\Raylib_TopsownShooter"
& "C:\raylib\w64devkit\bin\mingw32-make.exe" RAYLIB_PATH="C:/raylib/raylib" PROJECT_NAME=main BUILD_MODE=DEBUG
```
The executable is emitted as `main.exe` in the project root.

---

## Build (Web with emcc)
1. **Activate Emscripten**
   ```powershell
   & "C:\Users\bora2\emsdk\emsdk_env.ps1"
   ```

2. **Rebuild Raylib for the Web target** (ensures `libraylib.a` is wasm compatible):
   ```powershell
   Set-Location "C:\raylib\raylib\src"
   & "C:\raylib\w64devkit\bin\mingw32-make.exe" PLATFORM=PLATFORM_WEB
   ```

3. **Compile the game** (produces HTML + WASM bundle):
   ```powershell
   Set-Location "C:\Users\bora2\OneDrive\Desktop\GameDev Projects\Projects\Raylib_TopsownShooter"
   & "C:\raylib\w64devkit\bin\mingw32-make.exe" PLATFORM=PLATFORM_WEB `
       RAYLIB_PATH="C:/raylib/raylib" PROJECT_NAME=index BUILD_MODE=RELEASE
   ```

Artifacts land in the project root as `index.html`, `index.js`, `index.wasm`, `index.data`, plus `game_config.json` (see below).

---

## Serve & Play in a Browser
Use any static file server; Python is convenient:
```powershell
Set-Location "C:\Users\bora2\OneDrive\Desktop\GameDev Projects\Projects\Raylib_TopsownShooter"
python -m http.server 8000
```
Open <http://localhost:8000/index.html> in a modern browser.  
> **Important:** Upload `game_config.json` alongside the other web artifacts; the game fetches it via `emscripten_fetch`.

---

## Runtime Web Request / Configuration
On startup the game requests `game_config.json`:
- **Web build:** uses `emscripten_fetch` to load `/game_config.json` at runtime.
- **Desktop build:** uses Raylib’s `LoadFileText` to ingest the same file locally.

The JSON controls core gameplay values such as power‑up spawn cadence, enemy counts, drop rates, and the starting wave. If the fetch or read fails, built‑in defaults are used instead.

---

## Extras
- Animated preview: `Wavebreaker.gif`
- itch.io page: <https://bora0dev.itch.io/wavebreaker>

Bundle `index.*`, `index.data`, and `game_config.json` when deploying to itch.io or another static host.
