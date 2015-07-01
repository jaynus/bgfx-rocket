$input a_position, a_color0, a_texcoord0
$output v_texcoord0, v_color0

#include "..\examples\common\common.sh"

void main()
{
    vec2 p_position = a_position;
    gl_Position = mul(u_modelViewProj, vec4(p_position, 0.0, 1.0) );
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
}
