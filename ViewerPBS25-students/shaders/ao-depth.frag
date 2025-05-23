#version 330

uniform sampler2D def_depth;
uniform float near;
uniform float far;

in vec2 TexCoord; // Change to match vertex shader

out vec4 frag_color;

void main (void) {

    // float depth = texture(def_depth,TexCoord).r;
    vec3 col = texture(def_depth, TexCoord).rgb;
    // float l_depth = (2.0 * near) / (far + near - depth * (far - near));
    // frag_color = vec4(depth,depth,depth,1.0);
    frag_color = vec4(col, 1.0);
}
