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

float D(vec3 normal, vec3 h, float r){
    float s_roughness = r * r;
    float n_times_h = max(dot(normal,h),0.0);
    float s_n_times_h = n_times_h * n_times_h;

    float denominator = s_n_times_h * (s_roughness - 1) + 1;
    float f_denominator = PI * (denominator * denominator);
    float numerator = s_roughness;

    return numerator/f_denominator;
}

float G(vec3 normal, vec3 dir, float k){
    float n_times_dir = max(dot(normal,dir),0.0);
    float denominator = n_times_dir * (1 - k) + k;
    return n_times_dir/denominator;
}

/*
vec3 F(vec3 normal, vec3 v, vec3 F0){
    float n_times_v = max(dot(normal,v),0.0);
    n_times_v = 1 - n_times_v;
    return F0 + (1 - F0) * pow(n_times_v,5);
}
*/

vec3 F(vec3 v, vec3 h, vec3 F0){
    float cosTheta = max(dot(v, h), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

/*
vec3 F(vec3 normal, vec3 v, vec3 F0){
    float cosTheta = max(dot(normal, v), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
*/

void main (void) {
    float ks_v = 0.7;
    float kd_v = 0.3;
    float s_roughness_p1 = (roughness+1)*(roughness+1);
    float k_direct = s_roughness_p1 / 8;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Color, metalness);

    vec3 l = normalize(LightPos - FragPos);
    vec3 v = normalize(-FragPos);
    vec3 halfway = normalize(l+v);

    float dv = D(nm_Normal,halfway,roughness);
    float gv = G(nm_Normal,v,k_direct) * G(nm_Normal,l,k_direct);
    vec3 fv = F(v,halfway,F0);
    vec3 tv = vec3(dv,dv,dv);
    frag_color = vec4(fv,1.0);
}
