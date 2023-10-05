//Ported from preshack2/DrawStuff.cpp


#include "DrawStuff.hpp"
#include "ColorProgram.hpp"
#include "ColorTextureProgram.hpp"

#include "GLVertexArray.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>

#define POS_COL_DRAW( POS, COL, TYPENAME ) \
	static GLAttribBuffer< POS, COL > *TYPENAME ## _buffer = nullptr; \
	static Load< GLVertexArray > TYPENAME ## _buffer_for_color_program(LoadTagDefault, []() -> GLVertexArray * { \
	TYPENAME ## _buffer = new GLAttribBuffer< POS, COL >(); \
	return new GLVertexArray(GLVertexArray::make_binding(color_program->program, { \
		{color_program->Position_vec4, (*TYPENAME ## _buffer)[0]}, \
		{color_program->Color_vec4, (*TYPENAME ## _buffer)[1]} \
	})); \
}); \
void DrawStuff::draw(glm::mat4 const &world_to_clip, GLenum mode, size_t count, DrawStuff::TYPENAME const *verts) { \
	if (count == 0) return; \
	\
	TYPENAME ## _buffer->set((GLsizei)count, verts, GL_STREAM_DRAW); \
	\
	glUseProgram(color_program->program); \
	glUniformMatrix4fv(color_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip)); \
	glBindVertexArray(TYPENAME ## _buffer_for_color_program->array); \
	glDrawArrays(mode, 0, GLsizei(count)); \
	glBindVertexArray(0); \
	glUseProgram(0); \
}

POS_COL_DRAW( glm::vec2, glm::u8vec4, Pos2f_Col4ub )
POS_COL_DRAW( glm::vec3, glm::u8vec4, Pos3f_Col4ub )
POS_COL_DRAW( glm::vec4, glm::u8vec4, Pos4f_Col4ub )

POS_COL_DRAW( glm::vec2, glm::vec3, Pos2f_Col3f )
POS_COL_DRAW( glm::vec3, glm::vec3, Pos3f_Col3f )
POS_COL_DRAW( glm::vec4, glm::vec3, Pos4f_Col3f )

POS_COL_DRAW( glm::vec2, glm::vec4, Pos2f_Col4f )
POS_COL_DRAW( glm::vec3, glm::vec4, Pos3f_Col4f )
POS_COL_DRAW( glm::vec4, glm::vec4, Pos4f_Col4f )



#define POS_TEX_COL_DRAW( POS, TEX, COL, TYPENAME ) \
	static GLAttribBuffer< POS, TEX, COL > *TYPENAME ## _buffer = nullptr; \
	static Load< GLVertexArray > TYPENAME ## _buffer_for_color_texture_program(LoadTagDefault, []() -> GLVertexArray * { \
	TYPENAME ## _buffer = new GLAttribBuffer< POS, TEX, COL >(); \
	return new GLVertexArray(GLVertexArray::make_binding(color_texture_program->program, { \
		{color_texture_program->Position_vec4, (*TYPENAME ## _buffer)[0]}, \
		{color_texture_program->TexCoord_vec2, (*TYPENAME ## _buffer)[1]}, \
		{color_texture_program->Color_vec4, (*TYPENAME ## _buffer)[2]} \
	})); \
}); \
void DrawStuff::draw(glm::mat4 const &world_to_clip, GLenum mode, size_t count, DrawStuff::TYPENAME const *verts) { \
	if (count == 0) return; \
	\
	TYPENAME ## _buffer->set((GLsizei)count, verts, GL_STREAM_DRAW); \
	\
	glUseProgram(color_texture_program->program); \
	glUniformMatrix4fv(color_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip)); \
	glBindVertexArray(TYPENAME ## _buffer_for_color_texture_program->array); \
	glDrawArrays(mode, 0, GLsizei(count)); \
	glBindVertexArray(0); \
	glUseProgram(0); \
}

POS_TEX_COL_DRAW( glm::vec2, glm::vec2, glm::u8vec4, Pos2f_Tex2f_Col4ub )
POS_TEX_COL_DRAW( glm::vec2, glm::vec2, glm::vec4, Pos2f_Tex2f_Col4f )
