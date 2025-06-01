#version 330

uniform sampler2D def_normal;
uniform sampler2D def_depth;
uniform sampler2D noise_tex;

uniform float near;
uniform float far;
uniform float fov;
uniform float a_ratio;

uniform int num_samples;
uniform int num_directions;
uniform float radius;
uniform float vp_width;
uniform float vp_height;

uniform int use_noise;

in vec2 TexCoord;

out vec4 frag_color;

const float PI = 3.1415926538;

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

void main()
{
    float height   = near * tan(fov/2);
    float width    = a_ratio * height;
    vec2  tex_size = vec2(1.0/vp_width, 1.0/vp_height);

    vec3 normalSample = texture(def_normal,TexCoord).rgb;
    vec2 tile = vec2 (vp_width/8.0,vp_height/8.0);
    float noise_angle = 0.0;
    if (use_noise == 1)
        noise_angle = texture(noise_tex,TexCoord * tile).r * 2.0 * PI;

    if (normalSample != vec3(0.0)){

      vec3 FragPos = PosFromDepth(TexCoord,height,width);

      // ---------------------------------------------------------------------

      float theta = 0;
      float ao_term = 0.0;

      for (int i = 0; i < num_directions; ++i) {
          theta = i * (2*PI / num_directions) + noise_angle;
          vec2 dir = vec2(cos(theta),sin(theta));

          float a_tangent = atan(-(normalSample.x * dir.x + normalSample.y * dir.y), normalSample.z);
          float a_horizon = a_tangent;

          vec3 horizonPoint = FragPos;

          for (int j = 0; j < num_samples; ++j) {
              vec2 p_samp = TexCoord + (j+1) * (radius/num_samples) * dir;
              p_samp = clamp(p_samp,vec2(0.0),vec2(1.0));

              vec2 center_samp = floor(p_samp / tex_size + 0.5) * tex_size;
              vec3 eye_samp = PosFromDepth(center_samp,height,width);
              vec3 D = eye_samp - FragPos;

              float angle = atan(-D.z, length(D.xy));
              if (angle > a_horizon) {
                  a_horizon = angle;
                  horizonPoint = eye_samp;
              }
          }

          float dist = length(horizonPoint - FragPos);
          float attenuation = max(0.0, 1.0 - dist/radius);

          ao_term += (sin(a_horizon) - sin(a_tangent)) * attenuation;
      }

      float ao = 1.0 - (ao_term / (2.0 * PI * float(num_directions)));
      frag_color = vec4(ao,ao,ao,1.0);
    }
}
