#version 330

const float PI = 3.14159265359;
uniform float roughness;
uniform float metalness;

in vec3 Color;
in vec3 LightColor;
in vec3 LightPos;
in vec3 nm_Normal;
in vec3 FragPos;

out vec4 frag_color;

vec3 diffuse_part() {
    return Color/PI;
}

vec3 specular_part(){
    vec3 res = vec3(0.5,0.5,0.5);
    return res;
}

vec3 brdf_final_output(float kd, float ks) {
    return kd * diffuse_part() + ks * specular_part();
}

void main (void) {
    float ks_v = 0.7;
    float kd_v = 0.3;
    frag_color = vec4(brdf_final_output(kd_v,ks_v),1.0);
}
