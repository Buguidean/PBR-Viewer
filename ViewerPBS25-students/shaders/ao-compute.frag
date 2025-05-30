#version 330

uniform sampler2D def_normal;
uniform sampler2D def_depth;

uniform float near;
uniform float far;
uniform float fov;
uniform float a_ratio;

uniform int s_width;
uniform int s_height;
uniform int num_samples;
uniform int num_directions;
uniform float radius;

in vec2 TexCoord;
in vec3 Position;

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

    float eye_x = (Position.x * w * eye_z) / near;
    float eye_y = (Position.y * h * eye_z) / near;

    return vec3(eye_x,eye_y,eye_z);
}

void main()
{
    float height = near * tan(fov/2);
    float width = a_ratio * height;
    vec3 normalSample = texture(def_normal,TexCoord).rgb;
    if (normalSample != vec3(0.0)){

      vec3 FragPos = PosFromDepth(TexCoord,height,width);

      // ---------------------------------------------------------------------

      float theta = 0;
      float ao_term = 0.0;

      for (int i = 0; i < num_directions; ++i) {
          theta = i * (2*PI / num_directions);
          vec2 dir = vec2(cos(theta),sin(theta));

          float a_tangent = atan(-(normalSample.x * dir.x + normalSample.y * dir.y), normalSample.z);
          float a_horizon = a_tangent;

          for (int j = 0; j < num_samples; ++j) {
              vec2 samp_point = TexCoord + (radius/num_samples) * dir;
              samp_point = clamp(samp_point,vec2(0.0),vec2(1.0));
              vec3 eye_samp = PosFromDepth(samp_point,height,width);
              vec3 D = eye_samp - FragPos;
              a_horizon = max(a_horizon,atan(-D.z, length(D.xy)));
          }
          ao_term += sin(a_horizon) - sin(a_tangent);
      }

      float ao = 1.0 - (ao_term / (2.0 * PI * float(num_directions)));
      frag_color = vec4(ao,ao,ao,1.0);
    }
}
