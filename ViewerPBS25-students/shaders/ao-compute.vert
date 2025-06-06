#version 330

layout (location = 0) in vec3 vert;

out vec2 TexCoord;

void main(void) {
    gl_Position = vec4(vert, 1.0);
    TexCoord = vert.xy * 0.5 + 0.5;
}
