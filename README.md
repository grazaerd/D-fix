## The main issue in Dragon Nest
The game has a particle bug on the AMD CPU after the 64-bit release. This fork fixes it. Additionally, removing unnecessary calculation on low graphics.

## Why use this
.pak needs to be updated when dnshader changes (dnshaders2.dat). The dll checks if the specific shader is still the same hash. So updating the dll will be "rarely."

## List of Fixes
**High:**
- Particle fix for AMD CPUs
- Simplified Grass[^1]
- No VolumeFog and RadialBlur
- Simplified Volume Tex with little differences on color[^2]
- Fixed shadow[^3]
- Reduce shadow call on spherical map shader[^4] 

**Low:**
- Particle fix for AMD CPUs
- Simplified Volume Tex with little differences on color[^2]
- Simplified ground
- Simplified default
- Simplified Spherical map shader

## Note
Simplified[^5] in this context is hopefully less instruction count/ISA and GPU usage. And use Release if you're on high graphics, and ReleaseLowSettings for low settings. This should fix the stupidly high GPU usage on (Volume Tex) FRDN S2 Blue-Green Circle Mech (low FPS), Wind Mech in S3 (will cause stuttering) and others. I only follow SEA's changes, so if you're on another server, some shader might not work for you (.log file).

## Special Thanks
doitsujin (Original Author)

[^1]: Reduced shadow resolution.
[^2]: e.g. ML tornado.
[^3]: Shadow holes and fully connected the shadow.
[^4]: Biggest shader in the game and doing 2x shadow call for no reason.
[^5]: Removed (some) shadow on low graphics.