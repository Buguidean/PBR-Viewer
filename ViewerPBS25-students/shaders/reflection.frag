#version 330

uniform samplerCube skybox;
uniform mat4 inv_view;

in vec3 FragPos;			// Fragment position in ViewSpace
in vec3 nm_Normal;

out vec4 frag_color;

void main (void) {
    vec3 obsDir = normalize(FragPos);
    vec3 reflObsDir = reflect(obsDir,nm_Normal);
    mat3 rotation = mat3(inv_view);
    vec3 reflRayWorldSpace = rotation * reflObsDir;
    frag_color = vec4(textureLod(skybox,reflRayWorldSpace,0).rgb,1.0);
}
