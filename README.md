# GD Skin Studio (Android)

A Geometry Dash launcher (forked from the open-source Geode launcher) that
comes with one mod: **Skin Studio**.

- **Every icon unlocked**: all cubes, ships, balls, UFOs, waves, robots,
  spiders, swings, jetpacks, trails, death effects, ship fires and colours can
  be picked in the icon kit. Nothing is written to your save; turn the
  "Unlock every icon and colour" setting off and you're back to your real unlocks.
- **Photo icons**: in the icon kit, tap **Photos**, pick a gamemode tab, then
  **Import Photo**. Zoom, move, rotate, flip, pick a shape (square, rounded,
  circle) and an outline, name it and save. Your photo replaces that
  gamemode's icon in levels, the editor and the icon kit preview. Works for
  cube, ship, ball, UFO, wave, swing and jetpack (robot and spider are
  animated multi-part icons, so they keep their normal look).
  Photo icons only show on your screen; other players see your normal icon.

It uses your own Geometry Dash install (Play Store, version 2.2081). The game
itself is not included or modified.

## Get the APK
Every push to `main` builds the APK on GitHub Actions (~10 min) and publishes
it as a release named `build-<number>`. Newest build:
`https://github.com/<owner>/<repo>/releases/latest/download/gd-skin-studio.apk`

## Install on BlueStacks
Drag `gd-skin-studio.apk` onto the BlueStacks window (or use "Install APK"
in the side bar). Open **GD Skin Studio**; the first launch downloads the Geode
loader, then press Play. Use a 64-bit BlueStacks instance (Pie64 / Android 11).

To use photos from your PC, first copy them into BlueStacks with the Media
Manager (or drag them onto the window), then pick them with **Import Photo**.

It installs next to normal Geode and GD AI Learner, with its own separate mods
folder and settings.

## Layout
- `mod/`: the Skin Studio mod (C++, Geode SDK 5.11.2).
  - `src/unlock.cpp`: unlock-all hooks.
  - `src/image.cpp`: photo decoding, crop/zoom/rotate, shape mask and outline (stb_image).
  - `src/photos.cpp`: saved photo icons (PNGs in the mod's save folder) and gamemode assignments.
  - `src/apply.cpp`: draws photos on your player and the icon kit preview.
  - `src/ui.cpp`: the Photos button, photo manager and editor.
- `app/`: the Android launcher. Changes from upstream: app name, package id
  `com.william.gdskinstudio`, colours, the bundled mod is installed on every
  launch (`installBundledMods` in `GeometryDashActivity.kt`), and launcher
  self-updates are disabled so it never gets replaced by stock Geode.

Licensed under the upstream launcher's license (LICENSE.txt). stb is public domain.
