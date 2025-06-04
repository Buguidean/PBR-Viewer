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
out vec3 nm_Normal;
out vec3 worldNormal;
out vec3 LightColor;
out vec3 LightPos;
out vec3 FragPos;
out vec3 WorldPos;
out vec2 TexCoord;

void main(void)  {
    Color = vec3(1.0,1.0,0.0);
    nm_Normal = normalize(normal_matrix * normal);
    worldNormal = normalize(mat3(model) * normal);
    FragPos = vec3(view * model * vec4(vert,1.0));
    LightColor = vec3(0.7,0.7,0.7);
    LightPos = vec3(view * vec4(light,1.0));
    // Calculate world position for environment map sampling
    WorldPos = vec3(model * vec4(vert,1.0));
    TexCoord = texCoord;
    gl_Position = projection * view * model * vec4(vert,1.0);
}
