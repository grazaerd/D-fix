## The issue in Dragon Nest
The game has a particle bug on the AMD CPU after the 64-bit release. This fork fixes it.

## Why use this
.pak need to update when dnshader changes (dnshaders2.dat). The dll only needs to check if the specific shader is still the same hash. So updating the dll will be "rarely".

## Note
Simplified in this context is less instruction count and hopefully less GPU usage.