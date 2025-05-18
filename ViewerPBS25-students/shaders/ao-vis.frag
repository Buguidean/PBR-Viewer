#version 330

uniform sampler2D current_texture;

in vec3 TexCoord;

out vec4 frag_color;

void main (void) {
  frag_color = vec4(texture(current_texture, TexCoord.xy).rgb,1.0);
}
