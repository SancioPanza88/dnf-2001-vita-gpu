// Wall fragment shader — conversione palette P8->RGB in GPU via LUT.
// La CPU non espande mai texel: upload R8 + LUT 256x1, il resto è SGX.
precision mediump float;
varying vec2 v_uv;
varying float v_light;
uniform sampler2D u_tex;
uniform sampler2D u_pal;
void main() {
  float idx = texture2D(u_tex, v_uv).r;
  vec3 rgb = texture2D(u_pal, vec2(idx, 0.5)).rgb;
  gl_FragColor = vec4(rgb * v_light, 1.0);
}
