# OBS Studio 27.2.4 - Unofficial Modded CEF 109

This is an **unofficial** modification of OBS Studio 27.2.4 designed specifically to bring modern web compatibility (Chromium/CEF 109) to legacy operating systems like **Windows 8.1** and Windows Server 2012 R2.

## 🚀 Key Features & Fixes
* **Upgraded Browser Source Engine:** Upgraded from CEF 75/95 to **CEF 109** (Chrome 109), the absolute latest version of Chromium that supports Windows 8.1. This provides superior HTML5 rendering, modern JavaScript support, and better widget compatibility than even OBS 28's default browser!
* **Fixed D3D11 Shared Texture:** Patched `obs-browser` and `libobs-d3d11` to gracefully fallback to KMT handles (`OpenSharedResource`) when NT handles (`OpenSharedResource1`) fail on Windows 8.1, eliminating the dreaded "black screen" or blank browser source issue when Hardware Acceleration is enabled.
* **Fixed GPU Sandbox Crashes:** Added necessary Chromium flags (`--disable-gpu-sandbox`, `--ignore-gpu-blocklist`, `--use-angle=d3d11`) to prevent CEF from crashing on older Windows 8.1 graphics stacks.
* **Fixed Browser Dock Focus Stealing:** Backported focus handling fixes so that interactive browser docks no longer steal keyboard focus.
* **C++ ABI Crash Fix:** Integrated Visual C++ 2022 Redistributable DLLs (`msvcp140.dll`, etc.) directly into the `bin/64bit` directory to prevent `c0000005` Access Violation crashes during startup (`too_many_repeated_entries`) caused by mismatched C++ runtime versions on older servers.

## 📝 Disclaimer & Attribution
* **NOT OFFICIAL:** This repository is an unofficial, community-driven modification and is **not affiliated with, supported by, or endorsed by the official OBS Project**.
* **OBS Studio & Plugins:** The core OBS Studio source code and plugins belong entirely to the **OBS Project** contributors and are licensed under the GNU General Public License v2.0 (GPLv2).
* **CEF (Chromium Embedded Framework):** Belongs to the CEF project authors.
* **Modifications:** The custom backports, CEF integration patches, and Windows 8.1 compatibility fixes in this repository were implemented by **Truong Thanh Hieu** (@ubuntu2310fake) and Antigravity AI.

## 📥 Download & Usage
1. Go to the **Releases** tab.
2. Download the pre-compiled `OBS-27.2.4-Win8.1-Ready.zip`.
3. Extract to your Windows 8.1 server/machine.
4. Run `bin\64bit\obs64.exe`.
5. Enjoy full Browser Source Hardware Acceleration on Windows 8.1!

## 🛠️ Build Instructions
If you wish to build this from source:
1. You need **Visual Studio 2022** and **CMake**.
2. Run the provided PowerShell build script: `build_obs27_cef109.ps1` (or use CMake GUI).
3. The custom CEF 109 wrapper is automatically handled.
