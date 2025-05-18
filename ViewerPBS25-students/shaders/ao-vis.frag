#version 330

uniform sampler2D current_texture;

in vec2 TexCoord; // Change to match vertex shader

out vec4 frag_color;

void main (void) {
    frag_color = vec4(texture(current_texture, TexCoord).rgb, 1.0);
}
