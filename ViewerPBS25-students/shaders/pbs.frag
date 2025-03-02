#version 330

in vec3 LightColor;
in vec3 LightPos;
in vec3 nm_Normal;
in vec3 FragPos;

out vec4 frag_color;

void main (void) {
    frag_color = vec4(1.0,0.0,0.0,1.0);
}
