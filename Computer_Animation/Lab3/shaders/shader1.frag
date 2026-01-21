#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;

void main(){
    vec4 tex = texture(uTex, vUV);
    if(tex.a < 0.01) discard;
    FragColor = tex;
}
