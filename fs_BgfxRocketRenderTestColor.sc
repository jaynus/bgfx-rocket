$input v_texcoord0, v_color0

#include "..\examples\common\common.sh"

SAMPLER2D(s_texture0, 0);

void main()
{
	gl_FragColor = v_color0;
}
