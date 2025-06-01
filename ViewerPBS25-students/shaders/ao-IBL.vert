#version 330

layout (location = 0) in vec3 vert;

out vec2 TexCoord; // Changed to vec2
out vec3 Position;

uniform mat4 view;
uniform vec3 light;

out vec3 LightColor;
out vec3 LightPos;

void main(void) {
    LightColor = vec3(1.0,1.0,1.0);
    LightPos = vec3(view * vec4(light,1.0));
    // Map the vertex positions directly to screen coordinates
    gl_Position = vec4(vert, 1.0);
    Position = vert;
    // Generate texture coordinates from vertex positions
    // This maps [-1,1] to [0,1] for texture coordinates
    TexCoord = vert.xy * 0.5 + 0.5;
}
