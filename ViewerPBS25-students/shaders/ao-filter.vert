#version 330

layout (location = 0) in vec3 vert;

out vec2 TexCoord; // Changed to vec2
out vec3 Position;

void main(void) {
    // Map the vertex positions directly to screen coordinates
    gl_Position = vec4(vert, 1.0);
    Position = vert;
    // Generate texture coordinates from vertex positions
    // This maps [-1,1] to [0,1] for texture coordinates
    TexCoord = vert.xy * 0.5 + 0.5;
}
