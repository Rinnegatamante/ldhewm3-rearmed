# LDhewm3 ReARMed
<img src="shot.png"></img><br>
LDhewm3 ReARMed is a lowend fork of [dhewm3](https://dhewm3.org/) focusing primarily on tradeoff to ensure a decent enough graphical fidelity whilst being able to run on lowend devices.
Current focus is on a PSVita dedicated build.

## vitaGL flags

`HAVE_WRAPPED_ALLOCATORS=1 NO_DEBUG=1 DRAW_SPEEDHACK=1 CIRCULAR_POOL_SPEEDHACK=1 HAVE_SHADER_CACHE=1 DEPTH_STENCIL_HACK=1`

## Changelog

### v.0.5

- Made the ETC1 encoder multithreaded. This speeds up runtime texture compression for caching purposes.
- Updated to latest vitaGL and vitasdk.
- Optimized I/O accesses for faster loading of maps.
- Added packaged assets caching for faster loading of maps.
- Removed redunany entities and images loading. This reduces stutter during area transitions.
- Added buffering to I/O operations for faster loading of resources.
- Fixed a bug causing saves to become corrupted.
- Fixed a bug causing video cutscenes to not play correctly.
- Fixed a bug causing touch and rearpad presses to cause game crashes.
- Improved default settings setup for better performances.
- Improved the graphics backend pipeline to use less memory and use vitaGL more efficiently.
- Added an experimental frameskip option that can be enabled via com_frameSkip option in the ini.
- Added r_maxLights option to reduce the max number of active lights at the same time to greatly reduce CPU workload. Defaults to 8.
- Enabled LTO and made more small functions properly get inlined.
- Implemented native NEON usage for math operations.
- Made so that entities of all types that reside in deactivated areas of a map or behind portals get set in dormant state to not execute their physics, animation and AI steps every frame.
- Optimized the codepath that handles entities area updates.
- Added LOD system to reduce animations framerate for entities distant from the camera.
- Optimized dynamic lights interactions culling codepath.
- Fixed a bug in MD5 skinning that was causing some animations to break under certain circumstances.
- Enabled pre-decoding and caching for short looping SFX effects.
- Added caching and reduced tickrate for pathfinding of NPCs to improve CPU workload.
- Optimized flashlight dynamic light performances by reducing the frequency at which it updates its light beam.
- Added caching to flashlight beam to prevent needless recalculations of shadow volumes.
- Optimized box culling codepath.
- Implemented virtual keyboard for textbox interactions in the GUI.
- Added caching for objective images to reduce CPU workload when those are on screen.
- Optimized plane tests codepath.
- Disabled shadow casting for weapon muzzle flashes to prevent a shadow volume regeneration every frame.
- Restored sibling calls optimization.

## Credits

- emileb for [d3es-multithread](https://github.com/emileb/d3es-multithread) that was used as original base for this project.
