#version 330

uniform samplerCube skybox;
uniform mat4 inv_view;

in vec3 FragPos;			// Fragment position in ViewSpace
in vec3 nm_Normal;

out vec4 frag_color;

void main (void) {
    vec3 obsDir = normalize(FragPos);
    vec3 reflObsDir = reflect(obsDir,nm_Normal);
    vec3 reflRayWorldSpace = vec3(inv_view * vec4(reflObsDir,1.0));
    frag_color = vec4(texture(skybox,reflRayWorldSpace).rgb,1.0);
}
