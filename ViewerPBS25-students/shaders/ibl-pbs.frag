#version 330

const float PI = 3.14159265359;
uniform float roughness;
uniform float metalness;
uniform vec3 fresnel;
uniform int pbstex_use;

uniform sampler2D color_map;
uniform sampler2D roughness_map;
uniform sampler2D metalness_map;
uniform samplerCube diffuse_map;
uniform samplerCube specular_map;
uniform mat4 inv_view;

in vec3 Color;
in vec3 nm_Normal;
in vec3 FragPos;
in vec3 WorldPos;
in vec2 TexCoord;

out vec4 frag_color;

vec3 diffuse_part(vec3 color) {
    return color/PI;
}

float D(vec3 normal, vec3 h, float r) {
    // Prevent roughness from being exactly 0 to avoid numerical issues
    float r_clamped = max(r, 0.001);
    float s_roughness = r_clamped * r_clamped;
    float n_times_h = max(dot(normal, h), 0.0);
    float s_n_times_h = n_times_h * n_times_h;

    float denominator = s_n_times_h * (s_roughness - 1) + 1;
    float f_denominator = PI * (denominator * denominator);
    float numerator = s_roughness;

    return numerator / f_denominator;
}

float G(vec3 normal, vec3 dir, float k) {
    float n_times_dir = max(dot(normal, dir), 0.0);
    float denominator = n_times_dir * (1 - k) + k;
    return n_times_dir / denominator;
}

vec3 F(vec3 v, vec3 h, vec3 F0) {
    float cosTheta = max(dot(v, h), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Schlick's approximation of the Fresnel factor
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Schlick's approximation for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main(void) {
    vec3 u_Color;
    float u_roughness;
    float u_metalness;

    // Get material properties from either textures or uniforms
    if (pbstex_use == 0) {
        u_Color = Color;
        u_roughness = roughness;
        u_metalness = metalness;
    } else {
        u_Color = pow((texture(color_map, TexCoord)).rgb, vec3(2.2));
        u_roughness = (texture(roughness_map, TexCoord)).r;
        u_metalness = (texture(metalness_map, TexCoord)).r;
    }

    // Calculate view direction in world space
    vec3 viewPos = vec3(inv_view[3]);
    vec3 worldNormal = normalize(mat3(transpose(inverse(inv_view))) * nm_Normal);
    vec3 V = normalize(viewPos - WorldPos);

    // Calculate reflectance at normal incidence
    vec3 F0 = fresnel;
    F0 = mix(F0, u_Color, u_metalness);

    // DIFFUSE IBL
    // Sample the diffuse irradiance map
    vec3 irradiance = texture(diffuse_map, worldNormal).rgb;
    // Apply gamma correction to the environment map
    irradiance = pow(irradiance, vec3(2.2));

    // SPECULAR IBL
    vec3 R = reflect(-V, worldNormal);
    // Sample the specular environment map with LOD based on roughness
    vec3 specularIrradiance = textureLod(specular_map, R, u_roughness * 13.0).rgb;
    // Apply gamma correction to the environment map
    specularIrradiance = pow(specularIrradiance, vec3(2.2));

    // Calculate Fresnel term for IBL
    float NdotV = max(dot(worldNormal, V), 0.0);
    vec3 F = FresnelSchlickRoughness(NdotV, F0, u_roughness);

    // Calculate diffuse and specular factors
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - u_metalness;

    // Calculate the approximate geometry term for IBL
    float s_roughness_p1 = (u_roughness + 1.0) * (u_roughness + 1.0);
    float k_ibl = (u_roughness * u_roughness) / 2.0;
    float G_IBL = G(worldNormal, V, k_ibl) * G(worldNormal, worldNormal, k_ibl);

    // Combine diffuse and specular IBL contributions
    vec3 diffuse = irradiance * diffuse_part(u_Color);
    vec3 specular = specularIrradiance * (F * G_IBL * (4.0 * NdotV));

    vec3 ambient = kD * diffuse + specular;

    // Final color composition
    vec3 FinalColor = ambient;

    // Apply gamma correction
    vec3 gammaResult = pow(FinalColor, vec3(1.0/2.2));
    frag_color = vec4(gammaResult, 1.0);
}
