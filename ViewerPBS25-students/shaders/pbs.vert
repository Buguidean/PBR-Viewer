#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normal_matrix;
uniform vec3 light;

out vec3 Color;
out vec3 LightColor;
out vec3 LightPos;
out vec3 nm_Normal;
out vec3 FragPos;

void main(void)  {
    Color = vec3(0.7,0.3,0.4);
    LightColor = vec3(1.0,1.0,1.0);
    LightPos = vec3(view * vec4(light,1.0));
    nm_Normal = normalize(normal_matrix * normal);
    FragPos = vec3(view * model * vec4(vert,1.0));
    gl_Position = projection * view * model * vec4(vert,1.0);
}
