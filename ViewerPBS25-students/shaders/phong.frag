#version 330

in vec3 FragPos;			// Fragment position in ViewSpace
in vec3 nm_Normal;
in vec3 Color;
in vec3 LightColor;
in vec3 LightPos;			// Light position in ViewSpace

out vec4 frag_color;

void main (void) {
    vec3 lightDir = normalize(LightPos-FragPos);
    vec3 obsDir = normalize(-FragPos);
    vec3 reflLightDir = reflect(-lightDir,nm_Normal);

    float amb = 0.1;
    vec3 ambient = amb * LightColor;
    float diff = max(dot(nm_Normal,lightDir),0.0);
    vec3 diffuse = diff * LightColor;
    float spec = pow(max(dot(obsDir, reflLightDir), 0.0), 32);
    vec3 specular = spec * LightColor;

    vec3 Phong = (ambient + diffuse + specular) * Color;
    vec3 gammaResult = pow(Phong, vec3(1.0/2.2));
    frag_color = vec4(gammaResult,1.0);
}
