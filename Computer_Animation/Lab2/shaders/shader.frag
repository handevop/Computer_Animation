#version 330 core

in vec4 vCol;
out vec4 FragColor;

uniform sampler2D uTex;

void main()
{
    vec2 uv = gl_PointCoord;              // 0..1 unutar point sprite-a
    vec4 texel = texture(uTex, uv);       // RGBA iz PNG-a

    // uzmi i oblik (alpha) i boju iz teksture, a dodatno tintaj s vCol
    vec4 outCol = texel * vCol;

    // ako je alpha mala, odbaci piksel da ne vidiš kvadrat
    if (outCol.a < 0.01) discard;

    FragColor = outCol;
}
