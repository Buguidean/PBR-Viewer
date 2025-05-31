#version 330

uniform sampler2D ao_texture;

in vec2 TexCoord;

out vec4 frag_color;

void main (void) {
    float ao = texture(ao_texture,TexCoord).r;
    frag_color = vec4(ao, ao, ao, 1.0);
}
