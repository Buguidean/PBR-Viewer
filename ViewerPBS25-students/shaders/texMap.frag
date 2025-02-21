#version 330

uniform sampler2D current_texture;

in vec2 TexCoord;

out vec4 frag_color;

void main (void) {
    frag_color = texture(current_texture,TexCoord);
}
