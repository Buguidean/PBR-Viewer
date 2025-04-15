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

float D(vec3 normal, vec3 h, float r){
    // Prevent roughness from being exactly 0 to avoid numerical issues
    float r_clamped = max(r, 0.001);
    float s_roughness = r_clamped * r_clamped;
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

vec3 F(vec3 v, vec3 h, vec3 F0){
    float cosTheta = max(dot(v, h), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main (void) {
    float s_roughness_p1 = (roughness+1)*(roughness+1);
    float k_direct = s_roughness_p1 / 8;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Color, metalness);

    vec3 l = normalize(LightPos - FragPos);
    vec3 v = normalize(-FragPos);
    vec3 halfway = normalize(l+v);

    float distance = length(LightPos - FragPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = LightColor * attenuation;

    vec3 fv = F(v,halfway,F0);
    vec3 kS = fv;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    vec3 numerator = D(nm_Normal,halfway,roughness) * G(nm_Normal,l,k_direct) * G(nm_Normal,v,k_direct) * fv;
    float denominator = 4.0 * max(dot(nm_Normal,v),0.0) * max(dot(nm_Normal,l),0.0);
    vec3 specular = numerator / max(denominator,0.001);

    vec3 Lo = vec3(0,0,0);
    float NdotL = max(dot(nm_Normal, l), 0.0);
    Lo += (kD * diffuse_part() + specular) * radiance * NdotL;

    // Fix for ambient contribution: respect metalness in ambient term
    vec3 ambient = vec3(0.03);
    ambient *= mix(Color, F0, metalness);

    frag_color = vec4(Lo + ambient, 1.0);
}
