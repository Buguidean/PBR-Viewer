#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec2 TexCoord;

void main(void)  {
    FragPos = vec3(view * model * vec4(vert,1.0));
    TexCoord = texCoord;
    gl_Position = projection * view * model * vec4(vert,1.0);
}
