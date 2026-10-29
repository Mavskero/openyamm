$input v_texcoord0

#include "common.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_presentationGamma;

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    if (u_presentationGamma.x != 1.0)
    {
        color.rgb = pow(color.rgb, vec3_splat(u_presentationGamma.x));
    }
    gl_FragColor = color;
}
