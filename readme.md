# OpenRCT2-CE
OpenRCT2 for consoles.

## Currently available ports:
- PSVita [installation](#installation-vita) [Control scheme](#control-scheme-vita) [Building](#building-vita)

## installation vita:
- Get data.zip and openrct2.vpk from the releases tab.
- Extract data.zip into ux0:data (so that the folder structure is like this:)
```
ux0:data/
  -- OpenRCT2/
  -- -- g2.dat
  -- -- OpenRCT2/
  -- -- -- ....
  -- -- language/
  -- -- -- ....
```
- Get RollerCoaster Tycoon 2
- Extract the files to ux0:data/RCT2 (so that the folder structure is like this:)
```
ux0:data/
  -- RCT2/
  -- -- Data/
  -- -- ObjData/
  -- -- Tracks/
  ...
```
- Install the vpk
- And launch the game (first boot could take up to 5 minutes)
- Have fun!

## Control scheme vita:
- Cross: Left click
- Square: Right click
- Circle: Hold to speedup cursor
- Triangle: Close all windows

- Left stick: Move mouse cursor
- DPAD: Move viewport around
- Left shoulder button: Rotate view clockwise
- Right shoulder button: Rotate view counter-clockwise
- Start: Chat

## Building vita
(For developers, only linux as host system tested)
- Install [vitasdk](https://vitasdk.org)
- Install SDL2 and SDL2_image using `vdpm`
- In the root of the project, run:
```
cmake -Bbuild . -DCMAKE_BUILD_TYPE=Release -DENABLE_LIGHTFX=ON -DDISABLE_HTTP_TWITCH=ON
cd build
make -j8
./openrct2 sprite build g2.dat ../resources/g2/sprites.json

cd ..
cmake -Bbuild-vita -DCMAKE_BUILD_TYPE=Release -DDISABLE_NETWORK=OFF -DENABLE_LIGHTFX=ON -DDISABLE_HTTP_TWITCH=ON -DVITA=ON -DDISABLE_OPENGL=ON -DDISABLE_TTF=ON -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake .
cd build-vita
make -j8
```
- And you got the vpk in build-vita!

## Changelog
- Made sure ORCT2 compiles with latest GCC for both vita and Linux
- Made sure OpenSSL 3.0 and older can be used to compile network support
- Network support on PSVita
- Fast loading due to native sceIo functions
- Removed title sequence
- Added vita/controller control scheme
- Virtual mouse cursor with SDL
- Reduced maximum tool size (e.g. Modify land tool)
- Removed unused renderers on vita

# OpenRCT2 Original readme
An open-source re-implementation of RollerCoaster Tycoon 2. A construction and management simulation video game that simulates amusement park management.

---

![OpenRCT2.org Group Park 5](https://i.imgur.com/e7CK5Sc.png)

---

### Build Status
|             | Windows | Linux / Mac | Download |
|-------------|---------|-------------|----------|
| **master**  | [![AppVeyor](https://ci.appveyor.com/api/projects/status/7efnemxhon6i5n34/branch/master?svg=true)](https://ci.appveyor.com/project/IntelOrca/openrct2-ject9) | [![Travis CI](https://travis-ci.org/OpenRCT2/OpenRCT2.svg?branch=master)](https://travis-ci.org/OpenRCT2/OpenRCT2) | [![OpenRCT2.org](https://img.shields.io/badge/master-v0.1.2-green.svg)](https://openrct2.org/downloads/master/latest) |
| **develop** | [![AppVeyor](https://ci.appveyor.com/api/projects/status/7efnemxhon6i5n34/branch/develop?svg=true)](https://ci.appveyor.com/project/IntelOrca/openrct2-ject9) | [![Travis CI](https://travis-ci.org/OpenRCT2/OpenRCT2.svg?branch=develop)](https://travis-ci.org/OpenRCT2/OpenRCT2) | [![OpenRCT2.org](https://img.shields.io/badge/develop-v0.1.3-blue.svg)](https://openrct2.org/downloads/develop/latest) |

---

### Chat
You only need a GitHub or Twitter account to access these channels.

If you want to help *make* the game, join the developer channel.

If you need help, want to talk to the developers, or just want to stay up to date then join the non-developer channel for your language.

If you want to help translate the game to your language, please stop by the Localisation channel.

| Language | Non Developer | Developer | Localisation |
|----------|---------------|-----------|--------------|
| English | [![Gitter](https://img.shields.io/badge/gitter-general-blue.svg)](https://gitter.im/OpenRCT2/OpenRCT2/non-dev) | [![Gitter](https://img.shields.io/badge/gitter-development-yellowgreen.svg)](https://gitter.im/OpenRCT2/OpenRCT2) | [![Gitter](https://img.shields.io/badge/gitter-localisation-green.svg)](https://gitter.im/OpenRCT2/Localisation) |
| Nederlands | [![Gitter](https://img.shields.io/badge/gitter-general-blue.svg)](https://gitter.im/OpenRCT2/OpenRCT2/Nederlands) | | |

---

# Contents
- 1 - [Introduction](#1-introduction)
- 2 - [Downloading the game (pre-built)](#2-downloading-the-game-pre-built)
- 3 - [Building the game](#3-building-the-game)
  - 3.1 - [Building prerequisites](#31-building-prerequisites)
  - 3.2 - [Compiling and running](#32-compiling-and-running)
- 4 - [Contributing](#4-contributing)
  - 4.1 - [Bug fixes](#41-bug-fixes)
  - 4.2 - [New features](#42-new-features)
  - 4.3 - [Translation](#43-translation)
- 5 - [Licence](#5-licence)
- 6 - [More information](#6-more-information)
- 7 - [Sponsors](#7-sponsors)

---

# 1. Introduction

**OpenRCT2** is an open-source re-implementation of RollerCoaster Tycoon 2 (RCT2). The gameplay revolves around building and maintaining an amusement park containing attractions, shops and facilities. The player must try to make a profit and maintain a good park reputation whilst keeping the guests happy. OpenRCT2 allows for both scenario and sandbox play. Scenarios require the player to complete a certain objective in a set time limit whilst sandbox allows the player to build a more flexible park with optionally no restrictions or finance.

RollerCoaster Tycoon 2 was originally written by Chris Sawyer in x86 assembly and is the sequel to RollerCoaster Tycoon. The engine was based on Transport Tycoon, an older game which also has an equivalent open-source project, [OpenTTD](http://openttd.org). OpenRCT2 attempts to provide everything from RCT2 as well as many improvements and additional features, some of these include support for modern platforms, an improved interface, improved guest and staff AI, more editing tools, increased limits, and cooperative multiplayer. It also re-introduces mechanics from RollerCoaster Tycoon that were not present in RollerCoaster Tycoon 2. Some of those include; mountain tool in-game, the *"have fun"* objective, launched coasters (not passing-through the station) and several buttons on the toolbar.

---

# 2. Downloading the game (pre-built)

OpenRCT2 requires original files of RollerCoaster Tycoon 2 to play. It can be bought at either [Steam](http://store.steampowered.com/app/285330/) or [GOG.com](http://www.gog.com/game/rollercoaster_tycoon_2). If you have the original RollerCoaster Tycoon and its expansion packs, you can [point OpenRCT2 to these](https://github.com/OpenRCT2/OpenRCT2/wiki/Loading-RCT1-scenarios-and-data) in order to play the original scenarios.

[OpenRCT2.org](https://openrct2.org/downloads) offers precompiled builds and installers of the latest master and the develop branch. There is also a cross platform [Launcher](https://github.com/LRFLEW/OpenRCT2Launcher/releases) available that will automatically update your build of the game so that you always have the latest version.

Some Linux distributions offer native packages already. These packages are usually third-party, but we're trying to resolve issues they are facing.
* ArchLinux AUR: [openrct2-git](https://aur.archlinux.org/packages/openrct2-git) and [openrct2](https://aur.archlinux.org/packages/openrct2)
* Ubuntu PPA: [`master` branch](https://launchpad.net/~openrct2/+archive/ubuntu/master) and [`develop` branch](https://launchpad.net/~openrct2/+archive/ubuntu/nightly) (`develop` branch builds are temporarily on hold due to [missing functionality in bzr](https://bugs.launchpad.net/ubuntu/+source/bzr-git/+bug/1084403))
* openSUSE OBS: [games/openrct2](https://software.opensuse.org/download.html?project=games&package=openrct2)
* Gentoo (main portage tree): [games-simulation/openrct2](https://packages.gentoo.org/packages/games-simulation/openrct2)
* NixOS (`nixos-unstable` channel): [openrct2](https://github.com/NixOS/nixpkgs/blob/master/pkgs/games/openrct2/default.nix)

Some \*BSD operating systems offer native packages. These packages are usually third-party, but we're trying to resolve issues they are facing.
* OpenBSD: [games/openrct2](http://openports.se/games/openrct2)

---

# 3. Building the game

## 3.1 Building prerequisites

OpenRCT2 requires original files of RollerCoaster Tycoon 2 to play. It can be bought at either [Steam](http://store.steampowered.com/app/285330/) or [GOG.com](http://www.gog.com/game/rollercoaster_tycoon_2).

### Windows:
- 7 / 8 / 10
- Visual Studio 2017 update 3 (Enterprise / Professional / [Community (Free)](https://www.visualstudio.com/vs/community/))
  - Desktop development with C++
  - [Windows 10 SDK (10.0.14393.0)](https://go.microsoft.com/fwlink/p/?LinkId=838916)
- [7-Zip](http://www.7-zip.org/) (for deployment only)
- [NSIS](http://nsis.sourceforge.net/) (for deployment only)


### macOS:
- Xcode 8

The program can also be built as a command line program using CMake. This type of build requires:

- Xcode Command Line Tools
- [Homebrew](http://brew.sh)
- CMake (available through Homebrew)


### Linux:
- sdl2 (only for UI client)
- freetype (can be disabled)
- fontconfig (can be disabled)
- libzip (>= 1.0)
- libpng (>= 1.2)
- speexdsp (only for UI client)
- curl (only if building with http support)
- jansson (>= 2.5)
- openssl (>= 1.0; only if building with multiplayer support)
- iconv (part of glibc on Linux)
- zlib
- gl (commonly provided by Mesa or GPU vendors; only for UI client, can be disabled)
- cmake

---

## 3.2 Compiling and running
### Windows:
1. Check out the repository. This can be done using [GitHub Desktop](https://desktop.github.com) or [other tools](https://help.github.com/articles/which-remote-url-should-i-use).
2. Open a new Developer Command Prompt for VS 2017, then navigate to the repository (e.g. `cd C:\GitHub\OpenRCT2`).
3. Run `msbuild openrct2.proj /t:build /p:platform=x64`.
4. Run the game, `bin\openrct2`

Once you have ran msbuild once, further development can be done within Visual Studio by opening `openrct2.sln`.

Other examples:
```
set platform=x64
msbuild openrct2.proj /t:clean
msbuild openrct2.proj /t:rebuild /p:configuration=release
msbuild openrct2.proj /t:g2
msbuild openrct2.proj /t:PublishPortable
```

### macOS:
#### Xcode:
The recommended way of building OpenRCT2 for macOS is with Xcode. The Xcode build will create a self-contained application bundles which include all the necessary game files and dependencies. Open the project file OpenRCT2.xcodeproj in Xcode and build from there. Building this way will handle the dependencies for you automatically. You can also invoke an Xcode build from the command line using `xcodebuild`.

#### CMake:
A command line version of OpenRCT2 can be built using CMake. This type of build requires you to provide the dependencies yourself. The supported method of doing this is with [Homebrew](http://brew.sh). Once you have Homebrew installed, you can download all the required libraries with this command:
```
brew install cmake openssl jansson libpng sdl2 speexdsp libzip freetype pkg-config
```

Once you have the dependencies installed, you can build the project using CMake using the following commands:
```
mkdir build
cd build
cmake ..
make
ln -s ../data data
```
Then you can run the game by running `./openrct2`.

### Linux:
The standard CMake build procedure is to install the [required libraries](https://github.com/OpenRCT2/OpenRCT2#mac--linux), then:
```
mkdir build
cd build
cmake ../
make
```
Detailed instructions can be found on our [wiki](https://github.com/OpenRCT2/OpenRCT2/wiki/Building-OpenRCT2-on-Linux).

---

# 4. Contributing
OpenRCT2 uses the [gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows#gitflow-workflow). If you are implementing a new feature or logic from the original game, please branch off and perform pull requests to ```develop```. If you are fixing a bug for the next release, please branch off and perform pull requests to the correct release branch. ```master``` only contains tagged releases, you should never branch off this.

Please read our [contributing guidelines](https://github.com/OpenRCT2/OpenRCT2/blob/develop/CONTRIBUTING.md) for information.

## 4.1 Bug fixes
A list of bugs can be found on the [issue tracker](https://github.com/OpenRCT2/OpenRCT2/issues). Feel free to work on any bug and submit a pull request to the develop branch with the fix. Mentioning that you intend to fix a bug on the issue will prevent other people from trying as well.

## 4.2 New features
Please talk to the OpenRCT2 team first before starting to develop a new feature. We may already have plans or reasons against it, therefore contacting us will allow us to help you or prevent you from wasting any time. You can talk to us via gitter, see links at the top of this page.

## 4.3 Translation
You can translate the game into other languages by editing the language files in ```data/language``` directory. Please join discussions and submit pull requests to [OpenRCT2/Localisation](https://github.com/OpenRCT2/Localisation).

---

# 5. Licence
**OpenRCT2** is licensed under the GNU General Public License version 3.

---

# 6. More information
- [GitHub](https://github.com/OpenRCT2/OpenRCT2)
- [OpenRCT2.org](https://openrct2.org)
- [Forums](https://openrct2.org/forums/)
- [Facebook](https://www.facebook.com/OpenRCT2)
- [RCT subreddit](http://www.reddit.com/r/rct/)
- [OpenRCT2 subreddit](http://www.reddit.com/r/openrct2/)

## Similar Projects

| [OpenLoco](https://github.com/OpenRCT2/OpenLoco) | [OpenTTD](https://github.com/OpenTTD/OpenTTD) | [openage](https://github.com/SFTtech/openage) | [OpenRA](https://github.com/OpenRA/OpenRA) |
|:------------------------------------------------:|:----------------------------------------------------------------------------------------------------------:|:-------------------------------------------------------------------------------------------------------:|:-------------------------------------------------------------------------------------------------------------:|
| [![art128_wide](https://user-images.githubusercontent.com/550290/36566955-4833c9a2-1825-11e8-84b8-b4320888c322.png)](https://user-images.githubusercontent.com/550290/36566661-6a541128-1824-11e8-9633-8521d1a67d7e.png) | ![](https://github.com/OpenTTD/OpenTTD/raw/850d05d24d4768c81d97765204ef2a487dd4972c/media/openttd.128.png) | ![](https://user-images.githubusercontent.com/550290/36507534-4693f354-175a-11e8-93a7-faa0481474fb.png) | ![](https://raw.githubusercontent.com/OpenRA/OpenRA/bleed/packaging/linux/hicolor/128x128/apps/openra-ra.png) |
| Chris Sawyer's Locomotion | Transport Tycoon Deluxe | Age of Empires 2 | Red Alert |

# 7. Sponsors

Companies that kindly allow us to use their stuff:

| DigitalOcean | JetBrains | AppVeyor | Travis-CI |
|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [![do_logo_vertical_blue svg](https://user-images.githubusercontent.com/550290/36508276-8b572f0e-175c-11e8-8622-9febbce756b2.png)](https://www.digitalocean.com/) | [![jetbrains](https://user-images.githubusercontent.com/550290/36413299-0e0985ea-161e-11e8-8a01-3ef523b5905b.png)](https://www.jetbrains.com/) | [![AppVeyor](https://user-images.githubusercontent.com/550290/36508339-be413216-175c-11e8-97d8-760ced0931e8.png)](https://www.appveyor.com/) | [![Travis](https://raw.githubusercontent.com/travis-ci/docs-travis-ci-com/4b14eeab25ce8ca9164e177bfb60782a8535a822/images/travis-mascot-200px.png)](https://travis-ci.org/) |
| Hosting of various services | CLion and other products | MSVC CI | Linux + macOS CI |
