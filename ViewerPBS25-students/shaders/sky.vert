#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 TexCoord;

void main(void)  {
    TexCoord = vert;
    vec4 pos = projection * view * vec4(vert,1.0);
    gl_Position = pos.xyww;
}
