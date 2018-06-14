# skse64 plugin preloader

port of [skse plugin preloader](https://www.nexusmods.com/skyrim/mods/75795) for skse64/SSE

will also load dlls in SkyrimSE\CKPlugins\ into the CK

based on code by meh321 & sheson

in order to load after MO/MO2 VFS hooks the plugin load is delayed until _initterm_e is called, as meh321's [SSE dll loader](https://www.nexusmods.com/skyrimspecialedition/mods/10546/?) does

uses Nukem's [Detours library](https://github.com/Nukem9/detours) for IAT Hooking & [d3dx9_42.dll function implementations](https://github.com/Nukem9/SykrimSETest/blob/master/skyrim64_test/src/d3dx9_release.cpp) when hooking SkyrimSE.exe