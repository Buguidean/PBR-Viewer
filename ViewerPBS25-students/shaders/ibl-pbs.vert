#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normal_matrix;
uniform vec3 light;
uniform mat4 inv_view;

out vec3 Color;
out vec3 nm_Normal;
out vec3 FragPos;
out vec3 WorldPos;
out vec2 TexCoord;

void main(void) {
    Color = vec3(0.969, 0.0, 0.0);
    nm_Normal = normalize(normal_matrix * normal);
    FragPos = vec3(view * model * vec4(vert, 1.0));
    WorldPos = vec3(model * vec4(vert, 1.0));
    TexCoord = texCoord;
    gl_Position = projection * view * model * vec4(vert, 1.0);
}
