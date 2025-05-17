#version 330

layout (location = 0) out vec4 defColor;
layout (location = 1) out vec4 defNormal;
layout (location = 2) out vec4 defDepth;

uniform sampler2D current_texture;

in vec3 FragPos;
in vec2 TexCoord;

out vec4 frag_color;

void main (void) {
  frag_color = vec4(texture(current_texture, TexCoord).rgb,1.0);
}
