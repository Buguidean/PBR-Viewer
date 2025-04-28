#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normal_matrix;

out vec3 FragPos;
out vec3 nm_Normal;

void main(void)  {
    nm_Normal = normalize(normal_matrix * normal);
    FragPos = vec3(view * model * vec4(vert,1.0));
    gl_Position = projection * view * model * vec4(vert,1.0);
}
