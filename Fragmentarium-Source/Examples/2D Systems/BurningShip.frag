#group Burning Ship
#include "2DJulia.frag"
#include "Complex.frag"

//  Burning Ship Fractal
// (Implementation by Syntopia)

vec2 formula(vec2 z, vec2 c) {
	z = abs(z);
	z = cMul(z,z);
	z.y = -z.y;
	z += c;
	return z;
}

#preset Default
Center = -0.983448,0.033656
Zoom = 19.6131
AntiAliasScale = 1
AntiAlias = 3
Iterations = 33
PreIterations = 15
R = 1
G = 0.47059
B = 0.725
C = 1.64706
Julia = false
JuliaX = 0.23528
JuliaY = 5.5384
ShowMap = false
MapZoom = 2.25625
EscapeSize = 3.94031
ColoringType = 1
ColorFactor = 1
#endpreset