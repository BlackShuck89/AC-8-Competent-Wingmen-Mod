# No local development tools required — Windows build instructions

This project includes a GitHub Actions Windows build workflow. It builds the C++
planning library with Microsoft's compiler on GitHub's hosted PC. You do not
need Visual Studio, CMake, UE4SS, or a local compiler.

## Before you begin

**This is not a playable Ace Combat 8 mod.** The project lacks an in-game adapter
that identifies wingmen, reads missile/target status, and invokes verified game
APIs. A prebuilt DLL does not fix that. Do **not** copy the output DLL into the
Ace Combat 8 game directory expecting any changes.

## Build from your browser

1. Sign in to GitHub and create an empty repository (private is fine).
2. Extract this ZIP on your PC, then upload the project files and directories
   to your repository, preserving `.github/workflows/build-windows.yml`.
   If your browser doesn't upload hidden `.github` folders, create that file
   via the GitHub web editor in the same relative path and paste its contents.
3. Open your repository's **Actions** tab and choose **Build Windows Wingman Planner**.
   Enable workflows if GitHub requests it.
4. Click **Run workflow**, select your branch, and click **Run workflow** again.
5. After it succeeds, download **AC8-Win64-Compiled-Library-NOT-Playable-Mod**
   from its run page (the workflow stores a ZIP with the compiled DLL).

The compiled DLL is a native C++ **decision library** with an exported C API
for other software to call. It does not load itself into AC8 or fire missiles.

## What is still missing for a playable mod

An independently verified **offline single-player game integration layer**.
Its native loader and AI interfaces are not present here. You cannot solve
that just by compiling the existing project.
