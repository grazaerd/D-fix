## The issue in Dragon Nest
The game has a particle bug on the AMD CPU after the 64-bit release. This fork fixes it.

## Why use this
.pak need to update when dnshader changes (dnshaders2.dat). The dll checks if the specific shader is still the same hash. So updating the dll will be "rarely".

## List of Fixes
* Added Particle fix for AMD CPUs
* Added Simplified Grass
* Added No VolumeFog and RadialBlur
* Added Simplified Volume Tex with little differences on color (ex. ML tornado)

## Note
Simplified in this context is hopefully less instruction count/ISA and GPU usage.