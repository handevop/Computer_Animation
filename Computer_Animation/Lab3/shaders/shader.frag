#version 330 core

in vec2 vUV;
in vec4 vCol;
out vec4 FragColor;

uniform sampler2D uTex;

void main()
{
    vec4 tex = texture(uTex, vUV);
    FragColor = tex * vCol;

    if (FragColor.a < 0.01) discard;
}
