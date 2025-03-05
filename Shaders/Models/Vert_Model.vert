#version 430 core

// VBO-ból érkezõ változók
layout (location = 0 ) in vec3 vs_in_pos;
layout (location = 1 ) in vec3 vs_in_norm;
layout (location = 2 ) in vec3 vs_in_merged;
layout (location = 3 ) in vec2 vs_in_tex;

// a pipeline-ban tovább adandó értékek
out vec2 vs_out_tex;
out vec3 vs_out_norm;
out vec3 vs_out_merged;

void main()
{
	vs_out_norm = normalize(vs_in_norm);
	vs_out_merged = normalize(vs_in_merged);
	vs_out_tex = vs_in_tex;

	gl_Position = vec4(vs_in_pos, 1);
}