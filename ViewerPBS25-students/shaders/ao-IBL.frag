#version 330

const float PI = 3.14159265359;
uniform float roughness;
uniform float metalness;
uniform vec3 fresnel;
uniform int pbstex_use;
uniform int direct_light;

uniform float near;
uniform float far;
uniform float fov;
uniform float a_ratio;

uniform sampler2D def_albedo;
uniform sampler2D def_depth;
uniform sampler2D def_material;

uniform sampler2D def_normal;
uniform sampler2D ao_texture;

uniform samplerCube diffuse_map;
uniform samplerCube specular_map;

uniform mat4 inv_view;

in vec3 LightColor;
in vec3 LightPos;

in vec2 TexCoord;

out vec4 frag_color;

vec3 diffuse_part(vec3 color) {
    return color/PI;
}

float D(vec3 normal, vec3 h, float r) {
    float r_clamped = min(max(r, 0.01),0.99);
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

// Schlick's approximation for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float LinearizeDepth(in vec2 uv)
{
    float depth = texture(def_depth, uv).x;
    float z_ndc = 2.0 * depth - 1.0;
    float z_eye = 2.0 * near * far / (far + near - z_ndc * (far - near));
    return z_eye;
}

vec3 PosFromDepth(in vec2 uv, in float h, in float w)
{
    float eye_z = LinearizeDepth(uv);
    eye_z = -eye_z;

    vec2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    ndc.x = -ndc.x;

    float eye_x = ndc.x * w * eye_z / near;
    float eye_y = ndc.y * h * eye_z / near;

    return vec3(eye_x, eye_y, eye_z);
}

void main (void) {
    vec3 nm_Normal = texture(def_normal,TexCoord).rgb;
    if (nm_Normal != vec3(0.0)){

        vec3 u_Color;
        float u_roughness;
        float u_metalness;

        if (pbstex_use == 0)
        {
            u_Color = vec3(1.0,1.0,0.0);;
            u_roughness = roughness;
            u_metalness = metalness;
        }
        else
        {
            u_Color = pow((texture(def_albedo, TexCoord)).rgb, vec3(2.2));
            u_roughness = (texture(def_material, TexCoord)).r;
            u_metalness = (texture(def_material, TexCoord)).g;
        }

        float height   = near * tan(fov/2);
        float width    = a_ratio * height;
        vec3 FragPos   = PosFromDepth(TexCoord,height,width);
        vec3 WorldPos  = vec3(inv_view * vec4(FragPos,1.0));
        vec3 worldNormal = mat3(inv_view) * nm_Normal;
        vec3 viewPos = vec3(inv_view[3]);
        vec3 V = normalize(viewPos - WorldPos);

        vec3 F0 = fresnel;
        F0 = mix(F0, u_Color, u_metalness);

        // DIRECT LIGHTING CONTRIBUTION
        vec3 Lo = vec3(0.0);
        if (direct_light == 1){
            vec3 l = normalize(LightPos - FragPos);
            vec3 v = normalize(-FragPos);
            vec3 halfway = normalize(l+v);

            float distance = length(LightPos - FragPos);
            float attenuation = 1.0 / (distance * distance);
            vec3 radiance = LightColor * attenuation;

            // BRDF terms for direct lighting
            vec3 F_direct = F(v, halfway, F0);
            vec3 kS_direct = F_direct;
            vec3 kD_direct = vec3(1.0) - kS_direct;
            kD_direct *= 1.0 - u_metalness;

            float s_roughness_p1 = (u_roughness + 1.0) * (u_roughness + 1.0);
            float k_direct = s_roughness_p1 / 8.0;

            vec3 numerator = D(nm_Normal, halfway, u_roughness) *
                    G(nm_Normal, l, k_direct) *
                    G(nm_Normal, v, k_direct) *
                    F_direct;
            float denominator = 4.0 * max(dot(nm_Normal, v), 0.0) *
                    max(dot(nm_Normal, l), 0.0);
            vec3 specular = numerator / max(denominator, 0.001);

            float NdotL = max(dot(nm_Normal, l), 0.0);
            Lo += (kD_direct * diffuse_part(u_Color) + specular) * radiance * NdotL;
        }

        // AMBIENT/IBL CONTRIBUTION
        vec3 irradiance = texture(diffuse_map, worldNormal).rgb;
        irradiance = pow(irradiance, vec3(2.2));

        vec3 R = reflect(-V, worldNormal);
        vec3 specularIrradiance = textureLod(specular_map, R, u_roughness * 4.0).rgb;
        specularIrradiance = pow(specularIrradiance, vec3(2.2));

        float NdotV = max(dot(worldNormal, V), 0.0);
        vec3 F_ambient = FresnelSchlickRoughness(NdotV, F0, u_roughness);

        vec3 kS_ambient = F_ambient;
        vec3 kD_ambient = vec3(1.0) - kS_ambient;
        kD_ambient *= 1.0 - u_metalness;

        float k_ibl = (u_roughness * u_roughness) / 2.0;
        float G_IBL = G(worldNormal, V, k_ibl); // Simplified G term for IBL

        vec3 diffuse_ibl = irradiance * diffuse_part(u_Color);
        vec3 specular_ibl = specularIrradiance * (F_ambient * G_IBL) / (4.0 * NdotV);

        float ao = pow(texture(ao_texture,TexCoord).r,2.2);
        vec3 ambient = (kD_ambient * diffuse_ibl + specular_ibl) * ao;

        vec3 FinalColor = Lo + ambient;

        vec3 gammaResult = pow(FinalColor, vec3(1.0/2.2));
        frag_color = vec4(gammaResult, 1.0);
    }
    else{
        frag_color = vec4(texture(def_albedo,TexCoord).rgb,1.0);
    }
}
