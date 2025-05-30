#version 330

uniform sampler2D def_depth;
uniform float near;
uniform float far;

in vec2 TexCoord; // Change to match vertex shader

out vec4 frag_color;

float LinearizeDepth(in vec2 uv)
{
    float depth = texture(def_depth, uv).x;
    float z_ndc = 2.0 * depth - 1.0;
    float z_eye = 2.0 * near * far / (far + near - z_ndc * (far - near));
    return z_eye;
}

void main()
{
    // If we don't divide for color ouput we reach 1.0 to quick, so I divide by 4
    float c = LinearizeDepth(TexCoord) / 4.0;
    frag_color = vec4(c, c, c, 1.0);
}
