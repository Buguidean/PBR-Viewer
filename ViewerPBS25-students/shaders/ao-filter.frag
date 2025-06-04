#version 330 core

uniform sampler2D ao_tex;
uniform sampler2D def_depth;

uniform vec2 texelSize;
uniform float near;
uniform float far;
uniform int direction;

in vec2 TexCoord;
out vec4 frag_color;

const int blurRadius = 10;
const float blurSharpness = 40.0;

float LinearizeDepth(in vec2 uv)
{
    float depth = texture(def_depth, uv).x;
    float z_ndc = 2.0 * depth - 1.0;
    float z_eye = 2.0 * near * far / (far + near - z_ndc * (far - near));
    return z_eye;
}

void main() {
    float FragDepth = LinearizeDepth(TexCoord);
    float FragAO = texture(ao_tex, TexCoord).x;

    float sum = FragAO;
    float totalWeight = 1.0;

    for (int i = -blurRadius; i <= blurRadius; i++) {
        if (i == 0) continue;

        vec2 offset;
        if (direction == 0)
          offset = vec2(i, 0.0) * texelSize;
        else
          offset = vec2(0.0, i) * texelSize;

        vec2 sampleCoord = TexCoord + offset;

        float sampleDepth = LinearizeDepth(sampleCoord);
        float sampleAO = texture(ao_tex, sampleCoord).x;

        // Spatial weight (Gaussian)
        float distSquared = i*i;
        float spatialWeight = exp(-distSquared / (2.0 * blurRadius * blurRadius));

        // Depth weight
        float depthDiff = abs(FragDepth - sampleDepth);
        float depthWeight = exp(-depthDiff * blurSharpness);

        // Combined weight
        float weight = spatialWeight * depthWeight;

        sum += sampleAO * weight;
        totalWeight += weight;
    }

    // Normalize
    frag_color = vec4(sum/totalWeight, sum/totalWeight, sum/totalWeight, 1.0);
}
