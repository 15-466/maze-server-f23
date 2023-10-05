#include "DistanceTextProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< DistanceTextProgram > distance_text_program(LoadTagEarly);

DistanceTextProgram::DistanceTextProgram() {
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"in vec2 TexCoord;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"uniform vec4 TINT;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		//DEBUG:
		//"	fragColor = vec4(texture(TEX, texCoord).rgb, TINT.a);\n"
		//DEBUG:
		//"	vec2 grid = (texCoord * textureSize(TEX, 0));\n"
		//"	grid = max( vec2(0.0), -4.0 + 10.0 * abs(grid - round(grid)) );\n"
		//"	vec4 val = texture(TEX, texCoord);\n"
		//"	float s = 1.2*length(vec2(dFdx(val.a), dFdy(val.a)));\n"
		//"	float amt = (val.a - 0.5) / s + 0.5;\n"
		//"	fragColor = vec4(TINT.rgb, TINT.a * amt);\n"
		//"	s = 0.1;\n" //DEBUG
		//"	fragColor = vec4(grid.x+grid.y, (val.a - 0.5) / s, fract((val.g - val.r) * 25.5), TINT.a);\n"
		//"	fragColor = vec4(abs(dFdy(texCoord * textureSize(TEX,0))) * 10.0, 0.0, TINT.a);\n" //DEBUG

		//basic shader w/ edge AA:
		"	float val = texture(TEX, texCoord).a;\n"
		"	float slope = 1.2*length(vec2(dFdx(val), dFdy(val)));\n"
		"	float amt = (val - 0.5) / slope + 0.5;\n"
		"	fragColor = vec4(TINT.rgb, TINT.a * amt);\n"

		//actual old shader:
		//"	float amt = texture(TEX, texCoord).a;\n"
		//"	amt = smoothstep(0.5, 0.5, amt);\n"
		//"	fragColor = vec4(TINT.rgb, TINT.a * amt);\n"
		"}\n"
	);

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	TINT_vec4 = glGetUniformLocation(program, "TINT");
	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program);
	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0
	glUseProgram(0);
}
