#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform sampler2D mainTex;
uniform vec4 lCol;
uniform vec4 rCol;


void main()
{	
	vec4 mainColor = texture(mainTex, fsTex.xy);
    vec4 col = mainColor;

    //Red channel to color right lane
    col.xyz = vec3(.9) * rCol.xyz * vec3(mainColor.x);

    //Blue channel to color left lane
    col.xyz += vec3(.9) * lCol.xyz * vec3(mainColor.z);

    //Color green channel white
    col.xyz += vec3(.6) * vec3(mainColor.y);
    target = col;
}