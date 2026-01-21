#version 330 core

layout (location = 0) in vec3 aPos;   // iz VBO_pos
layout (location = 1) in vec4 aCol;   // iz VBO_col

out vec4 vCol;

uniform mat4 model;
uniform mat4 view;
uniform mat4 project;

uniform float uPointSize; // npr 32.0

void main()
{
    vCol = aCol;
    gl_Position = project * view * model * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
}
