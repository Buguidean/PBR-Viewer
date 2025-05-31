#version 330

layout (location = 0) out vec4 defColor;
layout (location = 1) out vec4 defNormal;

in vec3 nm_Normal;
in vec3 Color;
in vec2 TexCoord;

uniform int av_color;
uniform sampler2D color_map;

void main (void) {

  if (av_color == 0)
    defColor = vec4(Color,1.0);
  else
    defColor = texture(color_map,TexCoord);
  defNormal = vec4(nm_Normal,1.0);
}
