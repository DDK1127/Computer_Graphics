#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;

uniform mat4 uModel, uView, uProj;
out vec3 vN;
out vec3 vWPos;
out vec2 vUV;

void main(){
  vec4 wpos = uModel * vec4(aPos,1.0);
  vWPos = wpos.xyz;
  // normal uses upper-left 3x3 of model (no non-uniform scale here)
  vN = mat3(uModel) * aNrm;
  vUV = aUV;
  gl_Position = uProj * uView * wpos;
}
