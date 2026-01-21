#version 330 core

layout (location = 0) in vec2 aCorner;
layout (location = 1) in vec2 aUV;

layout (location = 2) in vec3 particleCenter;
layout (location = 3) in vec4 color;

out vec2 vUV;
out vec4 vCol;

uniform mat4 view;
uniform mat4 project;

uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform float uSize;

void main()
{
    vec3 worldPos = particleCenter + (aCorner.x * uCamRight + aCorner.y * uCamUp) * uSize;

    gl_Position = project * view * vec4(worldPos, 1.0);

    vUV = aUV;
    vCol = color;
}
