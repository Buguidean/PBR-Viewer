#version 330

uniform vec3 light;

in vec3 FragPos;			// Fragment position in ViewSpace
in vec3 nm_Normal;
in vec3 Color;
in vec3 LightColor;

out vec4 frag_color;

void main (void) {
    vec3 lightDir = normalize(light-FragPos);
    vec3 obsDir = normalize(-FragPos);
    vec3 reflLightDir = reflect(-lightDir,nm_Normal);

    float amb = 0.1;
    vec3 ambient = amb * LightColor;
    float diff = max(dot(nm_Normal,lightDir),0.0);
    vec3 diffuse = diff * LightColor;
    float spec = pow(max(dot(obsDir, reflLightDir), 0.0), 32);
    vec3 specular = spec * LightColor;

    vec3 Phong = (ambient + diffuse + specular) * Color;
    frag_color = vec4(Phong,1.0);
}
