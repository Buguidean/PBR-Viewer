#version 330

in vec3 TexCoord;

uniform samplerCube skybox;

out vec4 frag_color;

void main (void) {
    frag_color = texture(skybox,TexCoord);
}
