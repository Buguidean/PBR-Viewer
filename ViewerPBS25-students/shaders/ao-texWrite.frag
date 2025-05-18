#version 330

layout (location = 0) out vec4 defColor;
layout (location = 1) out vec4 defNormal;

in vec3 nm_Normal;
in vec3 Color;

void main (void) {
  defColor = vec4(Color,1.0);
  defNormal = vec4(nm_Normal,1.0);
}
