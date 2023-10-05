#pragma once

//Ported from preshack2/DrawStuff.hpp

/*
 * Helper to throw geometry at OpenGL.
 * not quite immediate mode, but close to it.
 *
 */

#include "GLBuffer.hpp"
#include <glm/glm.hpp>

#include <string>
#include <vector>

struct DrawStuff {

#define POS_COL_DRAW( POS, COL, TYPENAME ) \
	typedef GLAttribBuffer< POS, COL >::Vertex TYPENAME; \
	static void draw(glm::mat4 const &world_to_clip, GLenum mode, size_t count, TYPENAME const *verts); \
	static void draw(glm::mat4 const &world_to_clip, GLenum mode, std::vector< TYPENAME > const &verts) { \
		draw(world_to_clip, mode, verts.size(), verts.data()); \
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
#undef POS_COL_DRAW

#define POS_TEX_COL_DRAW( POS, TEX, COL, TYPENAME ) \
	typedef GLAttribBuffer< POS, TEX, COL >::Vertex TYPENAME; \
	static void draw(glm::mat4 const &world_to_clip, GLenum mode, size_t count, TYPENAME const *verts); \
	static void draw(glm::mat4 const &world_to_clip, GLenum mode, std::vector< TYPENAME > const &verts) { \
		draw(world_to_clip, mode, verts.size(), verts.data()); \
	}

	POS_TEX_COL_DRAW( glm::vec2, glm::vec2, glm::u8vec4, Pos2f_Tex2f_Col4ub )
	POS_TEX_COL_DRAW( glm::vec2, glm::vec2, glm::vec4, Pos2f_Tex2f_Col4f )
#undef POS_TEX_COL_DRAW


};
