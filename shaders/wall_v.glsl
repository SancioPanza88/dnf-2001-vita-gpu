// Wall vertex shader — gira su SGX, nessun vertice toccato dalla CPU oltre l'upload.
attribute vec3 a_pos;
attribute vec2 a_uv;
attribute float a_light;
varying vec2 v_uv;
varying float v_light;
uniform mat4 u_mvp;
void main() {
  v_uv = a_uv;
  v_light = a_light;
  gl_Position = u_mvp * vec4(a_pos, 1.0);
}
