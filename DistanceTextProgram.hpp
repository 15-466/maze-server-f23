#pragma once

//Note: from https://github.com/ixchow/preshack2

#include "Load.hpp"
#include "GL.hpp"

//Shader program that draws text from a signed distance field:
struct DistanceTextProgram {
	DistanceTextProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint OBJECT_TO_CLIP_mat4 = -1U;
	GLuint TINT_vec4 = -1U;

	//Textures:
	//TEXTURE0 - texture that is accessed by TexCoord
};

extern Load< DistanceTextProgram > distance_text_program;
