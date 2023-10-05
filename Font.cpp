//Adapted from Font.cpp in from https://github.com/ixchow/preshack2

#include "Font.hpp"
#include "DistanceTextProgram.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include "data_path.hpp"
#include "Load.hpp"
#include "GL.hpp"
#include "gl_errors.hpp"

#include "GLVertexArray.hpp"
#include "GLBuffer.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <unordered_map>
#include <iostream>
#include <map>
#include <algorithm>
#include <chrono>

constexpr uint32_t RENDER_HEIGHT = 80; //size of glyphs when rendered to texture
constexpr uint32_t OVERSAMPLE = 11; //oversample glyphs be OVERSAMPLE x OVERSAMPLE when making SDF
constexpr uint32_t SDF_MARGIN = 4; //extra pixels to add to outside of bitmaps for SDF

static GLAttribBuffer< glm::vec2, glm::vec2 > *text_buffer = nullptr;
static Load< GLVertexArray > text_buffer_for_distance_text_program(LoadTagDefault, []() -> GLVertexArray * {
	text_buffer = new GLAttribBuffer< glm::vec2, glm::vec2 >();
	return new GLVertexArray(GLVertexArray::make_binding(distance_text_program->program, {
		{distance_text_program->Position_vec4, (*text_buffer)[0]},
		{distance_text_program->TexCoord_vec2, (*text_buffer)[1]}
	}));
});

//hb usage based on simple example in manual:
//  https://harfbuzz.github.io/ch03s03.html
//ft usage based on simple example in manual:
//  https://www.freetype.org/freetype2/docs/tutorial/step1.html

static FT_Library library = ([](){
	FT_Library temp;
	FT_Error err = FT_Init_FreeType( &temp );
	if (err != FT_Err_Ok) {
		std::cerr << "Failed to initialize FreeType:" << std::endl;
		std::cerr << FT_Error_String(err);
		exit(1);
	}
	return temp;
})();

struct Glyph {
	Glyph() {
	}
	Glyph(FT_GlyphSlotRec *_gs) {
		assert(_gs);
		auto &gs = *_gs;


		//extra a simple inside/outside bitmap from the glyph:
		//(note: origin lower left)
		glm::uvec2 inside_size = glm::uvec2(gs.bitmap.width, gs.bitmap.rows);
		//location of inside's lower left corner:
		glm::vec2 inside_offset(
			gs.bitmap_left,
			gs.bitmap_top-int32_t(gs.bitmap.rows)
		);
		//make sure dimensions are a multiple of OVERSAMPLE by padding upper right:
		while (inside_size.x % OVERSAMPLE) inside_size.x += 1;
		while (inside_size.y % OVERSAMPLE) inside_size.y += 1;

		std::vector< uint8_t > inside(inside_size.x * inside_size.y, 0x0);

		if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_NONE) {
			std::cerr << "WARNING: ignoring FT_PIXEL_MODE_NONE bitmap." << std::endl;
			return;
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
			uint8_t const *data = gs.bitmap.buffer + (gs.bitmap.pitch < 0 ? gs.bitmap.pitch * -gs.bitmap.rows : 0);
			for (uint32_t r = 0; r < gs.bitmap.rows; ++r) {
				uint8_t const *px = reinterpret_cast< uint8_t const * >(data);
				for (uint32_t c = 0; c < gs.bitmap.width; ++c) {
					int32_t il = ( ((px[c/8]) & (1 << (7 - (c%8)))) ? 255 : 0);
					inside[(gs.bitmap.rows - 1 - r) * inside_size.x + c] = (uint8_t)il;
				}
				data += gs.bitmap.pitch;
			}
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
			uint8_t const *data = gs.bitmap.buffer + (gs.bitmap.pitch < 0 ? gs.bitmap.pitch * -gs.bitmap.rows : 0);
			for (uint32_t r = 0; r < gs.bitmap.rows; ++r) {
				uint8_t const *px = reinterpret_cast< uint8_t const * >(data);
				for (uint32_t c = 0; c < gs.bitmap.width; ++c) {
					float l = float(px[c]) / float(gs.bitmap.num_grays - 1);
					int32_t il = std::max(0, std::min(255, int32_t(255.0f * l)));
					inside[(gs.bitmap.rows - 1 - r) * inside_size.x + c] = (uint8_t)il;
				}
				data += gs.bitmap.pitch;
			}
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_GRAY2) {
			std::cerr << "WARNING: ignoring FT_PIXEL_MODE_GRAY2 bitmap." << std::endl;
			return;
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_GRAY4) {
			std::cerr << "WARNING: ignoring FT_PIXEL_MODE_GRAY4 bitmap." << std::endl;
			return;
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_LCD) {
			std::cerr << "WARNING: ignoring FT_PIXEL_MODE_LCD bitmap." << std::endl;
			return;
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_LCD_V) {
			std::cerr << "WARNING: ignoring FT_PIXEL_MODE_LCD_V bitmap." << std::endl;
			return;
		} else if (gs.bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
			std::cerr << "WARNING: ignoring FT_PIXEL_MODE_BGRA bitmap." << std::endl;
			return;
		} else {
			std::cerr << "WARNING: ignoring unknown pixel_mode bitmap." << std::endl;
			return;
		}

		//make signed distance field:
		glm::uvec2 sdf_size;
		sdf_size.x = inside_size.x / OVERSAMPLE + 2U * SDF_MARGIN;
		sdf_size.y = inside_size.y / OVERSAMPLE + 2U * SDF_MARGIN;

		glm::vec2 sdf_offset = inside_offset / float(OVERSAMPLE) - glm::vec2(SDF_MARGIN);
		std::vector< float > sdf(sdf_size.x * sdf_size.y, 0.0f);

		//TODO: compute distance somehow!

//#define JUST_ANTIALIAS
#ifdef JUST_ANTIALIAS
		constexpr float SCALE = 255.0f / 1.0f;
		constexpr float OFFSET = 0.0;
		//for now: average of inside-ness per pixel:
		for (uint32_t y = 0; y < sdf_size.y; ++y) {
			int32_t begin_y = (int32_t(y) - SDF_MARGIN) * OVERSAMPLE;
			int32_t end_y = begin_y + OVERSAMPLE;
			for (uint32_t x = 0; x < sdf_size.x; ++x) {
				int32_t begin_x = (int32_t(x) - SDF_MARGIN) * OVERSAMPLE;
				int32_t end_x = begin_x + OVERSAMPLE;

				//average overlapped:
				uint32_t in = 0;
				for (int32_t iy = begin_y; iy < end_y; ++iy) {
					if (uint32_t(iy) >= inside_size.y) continue;
					for (int32_t ix = begin_x; ix < end_x; ++ix) {
						if (uint32_t(ix) >= inside_size.x) continue;
						if (inside[iy * inside_size.x + ix] != 0) in += 1;
					}
				}
				sdf[y * sdf_size.x + x] = in / float(OVERSAMPLE * OVERSAMPLE);

				/*
				//just sample middle:
				int32_t mid_y = (begin_y + end_y) / 2;
				int32_t mid_x = (begin_x + end_x) / 2;
				sdf[y * sdf_size.x + x] = 0;
				if (uint32_t(mid_y) < inside_size.y
				 && uint32_t(mid_x) < inside_size.x
				 && inside[mid_y * inside_size.x + mid_x] != 0) {
					sdf[y * sdf_size.x + x] = 255;
				}
				*/
			}
		}
#endif //JUST_ANTIALIAS

#define CONE_DISTANCE
#ifdef CONE_DISTANCE
	//dis * SCALE + OFFSET, clamped to [0,255] is the actual sdf texture value:
	constexpr float SCALE = 16.0;
	constexpr float OFFSET = 127.5;

	//build table of cone steps: relative offset and distance value, in sorted order:
	//NOTE: could shrink by storing only part and taking negatives
	//grr MSVC --> constexpr int16_t ConeRadius = 4 * OVERSAMPLE;
	#define ConeRadius (int16_t(4 * OVERSAMPLE))
	static std::vector< std::pair< float, glm::i16vec2 > > cone_steps = [](){
		std::vector< std::pair< float, glm::i16vec2 > > steps;
		constexpr uint32_t Count = (2*uint32_t(ConeRadius)+1)*(2*uint32_t(ConeRadius)+1);
		steps.reserve(Count);
		for (int16_t dy = -ConeRadius; dy <= ConeRadius; ++dy) {
			for (int16_t dx = -ConeRadius; dx <= ConeRadius; ++dx) {
				float dis2 = float(dx)*float(dx) + float(dy)*float(dy);
				if (dis2 <= ConeRadius * ConeRadius) {
					steps.emplace_back(std::sqrt(dis2) / float(OVERSAMPLE), glm::i16vec2(dx,dy));
				}
			}
		}
		assert(steps.size() <= Count); //not == since the ones outside the circle are clipped

		std::stable_sort(steps.begin(), steps.end(), [](auto const &a, auto const &b){
			return a.first < b.first;
		});
		return steps;
	}();

	sdf.assign(sdf_size.x * sdf_size.y, std::numeric_limits< float >::quiet_NaN());

	{ //outside:
		std::vector< uint32_t > todo;
		todo.reserve(sdf_size.x * sdf_size.y);
		for (uint32_t y = 0; y < sdf_size.y; ++y) {
			for (uint32_t x = 0; x < sdf_size.x; ++x) {
				glm::ivec2 px = (glm::ivec2(x,y) - glm::ivec2(SDF_MARGIN)) * glm::ivec2(OVERSAMPLE) + glm::ivec2(OVERSAMPLE / 2);
				bool px_inside = (
					   px.y >= 0 && px.y < int32_t(inside_size.y)
					&& px.x >= 0 && px.x < int32_t(inside_size.x)
					&& inside[px.y * inside_size.x + px.x] != 0
				);
				todo.emplace_back((x << 16) | (y << 1) | (px_inside ? 1 : 0));
			}
		}

		for (auto const &[dis, step] : cone_steps) {
			uint32_t o = 0;
			for (uint32_t i = 0; i < todo.size(); ++i) {
				uint32_t packed = todo[i];
				bool px_inside = ((packed & 1) != 0);
				uint32_t x = (packed >> 16);
				uint32_t y = ((packed & 0xfffe) >> 1);
				glm::ivec2 px = (glm::ivec2(x,y) - glm::ivec2(SDF_MARGIN)) * glm::ivec2(OVERSAMPLE) + glm::ivec2(OVERSAMPLE / 2);
				//pixel that gets to this one via the cone step in question:
				glm::ivec2 src = px - glm::ivec2(step);
	
				bool src_inside = (
					    src.y >= 0 && src.y < int32_t(inside_size.y)
					 && src.x >= 0 && src.x < int32_t(inside_size.x)
					 && inside[src.y * inside_size.x + src.x] != 0
				);

				if (!px_inside && src_inside) {
					sdf[y * sdf_size.x + x] =-dis;
				} else if (px_inside && !src_inside) {
					sdf[y * sdf_size.x + x] = dis;
				} else {
					//write back for later:
					todo[o] = packed;
					o += 1;
				}
			}
			todo.resize(o);
			if (todo.empty()) break;
		}

		//everything else gets maximum distance outside/inside:
		for (uint32_t i = 0; i < todo.size(); ++i) {
			uint32_t packed = todo[i];
			bool px_inside = ((packed & 1) != 0);
			uint32_t x = (packed >> 16);
			uint32_t y = ((packed & 0xfffe) >> 1);

			sdf[y * sdf_size.x + x] = (px_inside ? 1.0f : -1.0f) * 255.0f / SCALE; //WAS: cone_steps.back().first;
		}
	}
#endif

#ifdef MARCHING_DISTANCE
		std::vector< std::pair< uint32_t, glm::u16vec2 > > distance_close(inside_size.x * inside_size.y, -1U);
		std::map< uint32_t, glm::u16vec2 > todo;

		{ //inside
			for (uint32_t y = 0; y < inside_size.y; ++y) {
				for (uint32_t x = 0; x < inside_size.x; ++x) {
					if (inside[y * inside_size.x + x] != 0) {
						distance_close[y * inside_size.x + x] = std::make_pair(0, glm::u16vec2(x,y));
						todo.emplace(0, glm::u16vec2(x,y));
					}
				}
			}
			while (!todo.empty()) {
				glm::ivec2 at = glm::ivec2(todo.begin()->second);
				uint32_t dis = todo.begin()->second;
				todo.erase(todo.begin());
				if (distance_close[at.y * inside_size.x + at.x] < dis) continue;
				assert(distance_close[at.y * inside_size.x + at.x] == dis);

				for (int32_t dy = -1; dy <= 1; ++dy) {
					if (dy + at.y < 0 || dy + at.y >= int32_t(inside_size.y))
					for (int32_t dx = -1; dx <= 1; ++dx) {
						//...
					}
				}
			}
		}
#endif //MARCHING_DISTANCE


		//convert sdf to stored bitmap:
		size = sdf_size;
		offset = sdf_offset;
		bitmap.assign(sdf_size.x * sdf_size.y, glm::u8vec4(0xff, 0x00, 0x88, 0xff));

		for (uint32_t y = 0; y < sdf_size.y; ++y) {
			for (uint32_t x = 0; x < sdf_size.x; ++x) {
				uint32_t i = y * sdf_size.x + x;
				float r = std::max(0.0f,-sdf[i] * SCALE);
				float g = std::max(0.0f, sdf[i] * SCALE);
				float b = (float)x;
				float a = sdf[i] * SCALE + OFFSET;
				bitmap[i] = glm::u8vec4(
					std::max(0, std::min(255, int32_t(std::round(r)))),
					std::max(0, std::min(255, int32_t(std::round(g)))),
					std::max(0, std::min(255, int32_t(std::round(b)))),
					std::max(0, std::min(255, int32_t(std::round(a))))
				);
			}
		}
	}
	glm::uvec2 size = glm::uvec2(0); //size in pixels
	std::vector< glm::u8vec4 > bitmap; //lower-left origin
	glm::vec2 offset = glm::vec2(0); //offset from pen to bottom left of bitmap

	//origin of bitmap in packed texture:
	glm::uvec2 packed_origin = glm::uvec2(-1U);

	static Glyph missing(FT_UInt glyph_index) {
		Glyph ret;
		//TODO: some fancy pattern or something
		return ret;
	};

};

struct FontInternals {
	FontInternals(std::string const &_path) : path(_path) {
		FT_Error err = FT_New_Face( library, path.c_str(), 0, &face );
		if (err != FT_Err_Ok) {
			throw std::runtime_error("Error opening font '" + path + "': " + std::string(FT_Error_String(err)));
		}

		err = FT_Set_Pixel_Sizes( face, 0, RENDER_HEIGHT * OVERSAMPLE );
		if (err != FT_Err_Ok) {
			throw std::runtime_error("Error setting font size for '" + path + "': " + std::string(FT_Error_String(err)));
		}

		//DEBUG:
		std::cout << "Font '" << path << "' has nominal height " << RENDER_HEIGHT << " with " << OVERSAMPLE << "x oversampling and:\n";
		std::cout << "\tascender " << face->size->metrics.ascender / 64.0f << "\n";
		std::cout << "\tdescender " << face->size->metrics.descender / 64.0f << "\n";
		std::cout << "\theight " << face->size->metrics.height / 64.0f << "\n";
		std::cout << "\ty_ppem " << face->size->metrics.y_ppem << "\n";
		std::cout.flush();
			

		font = hb_ft_font_create(face, nullptr);
#define PRELOAD
#ifdef PRELOAD
		{ //preload all of printable ascii:
			auto before = std::chrono::high_resolution_clock::now();
			std::cout << "  pre-rendering ASCII glyphs..."; std::cout.flush();

			std::vector< FT_UInt > to_load;
			for (char c = ' '; c <= '~'; ++c) {
				shape_text(std::string(&c, 1), &to_load, nullptr, nullptr);
			}
			upload_glyphs(to_load);

			auto after = std::chrono::high_resolution_clock::now();

			std::cout << " rendered " << rendered.size() << " in " << std::chrono::duration< double, std::ratio< 1 > >(after - before).count() << "s." << std::endl;
		}
#endif
	}

	~FontInternals() {
		if (packed_tex) {
			glDeleteTextures(1, &packed_tex);
			packed_tex = 0;
		}
		if (font) {
			hb_font_destroy(font);
			font = nullptr;
		}
		FT_Done_Face(face);
	}

	std::string path;
	FT_Face face;
	hb_font_t *font = nullptr;

	std::unordered_map< FT_UInt, Glyph > rendered;

	GLuint packed_tex = 0;
	glm::uvec2 packed_size = glm::uvec2(0);
	bool packed_dirty = false;

	void upload_glyphs(std::vector< FT_UInt > const &glyph_indices) {
		for (auto const &glyph_index : glyph_indices) {
			if (rendered.count(glyph_index)) continue;
			FT_Error err = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_HINTING /* | FT_LOAD_COLOR */ | FT_LOAD_MONOCHROME);
			if (err != FT_Err_Ok) {
				std::cerr << "WARNING: error loading glyph index " << glyph_index << " for '" << path << "': " << FT_Error_String(err) << std::endl;
				rendered.emplace(glyph_index, Glyph::missing(glyph_index));
				packed_dirty = true;
				continue;
			}

			if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
				err = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_MONO /*FT_RENDER_MODE_NORMAL*/ );
				if (err != FT_Err_Ok) {
					std::cerr << "WARNING: error rendering glyph index " << glyph_index << " for '" << path << "': " << FT_Error_String(err) << std::endl;
					rendered.emplace(glyph_index, Glyph::missing(glyph_index));
					packed_dirty = true;
					continue;
				}
			}

			rendered.emplace(glyph_index, face->glyph);
			packed_dirty = true;
		}
		if (packed_dirty) {
			packed_dirty = false;
			std::vector< std::pair< FT_UInt, Glyph * > > to_pack;
			to_pack.reserve(rendered.size());
			for (auto & [idx, glyph] : rendered) {
				to_pack.emplace_back(idx, &glyph);
			}
			std::sort(to_pack.begin(), to_pack.end(), [](std::pair< FT_UInt, Glyph * > const &a, std::pair< FT_UInt, Glyph * > const &b){
				if (a.second->size.y != b.second->size.y) return a.second->size.y > b.second->size.y;
				if (a.second->size.x != b.second->size.x) return a.second->size.x > b.second->size.x;
				return a.first < b.first;
			});

			packed_size = glm::uvec2(1024U,128U);

			//expand for character size:
			for (auto & [idx, gp] : to_pack) {
				while (gp->size.x > packed_size.x) packed_size.x *= 2;
			}

			uint32_t padding = 4;
			glm::uvec2 at(0,0);
			uint32_t row_height = 0;
			for (auto & [idx, gp] : to_pack) {
				if (at.x + gp->size.x > packed_size.x) {
					at.x = 0;
					at.y += row_height + padding;
					row_height = 0;
				}
				row_height = std::max(row_height, gp->size.y);
				while (at.y + row_height > packed_size.y) packed_size.y *= 2;
				gp->packed_origin = at;

				at.x += gp->size.x + padding;
			}

			//std::cout << "Packed size: "<< packed_size.x << ", " << packed_size.y << std::endl;

			//copy glyphs to packed data:
			std::vector< glm::u8vec4 > packed_data(packed_size.x * packed_size.y, glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			for (auto & [idx, gp] : to_pack) {
				for (uint32_t y = 0; y < gp->size.y; ++y) {
					for (uint32_t x = 0; x < gp->size.x; ++x) {
						packed_data[(gp->packed_origin.y + y) * packed_size.x + (gp->packed_origin.x + x)] = gp->bitmap[y * gp->size.x + x];
					}
				}
			}

			//upload texture:
			if (packed_tex == 0) glGenTextures(1, &packed_tex);
			glBindTexture(GL_TEXTURE_2D, packed_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, packed_size.x, packed_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed_data.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			//DEBUG:
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);

			GL_ERRORS();

		}
	}

	void shape_text(std::string const &utf8,
		std::vector< FT_UInt > *_indices,
		std::vector< glm::vec2 > *_positions,
		std::vector< glm::vec2 > *_cursors_after) {

		hb_buffer_t *buf = hb_buffer_create();
		hb_buffer_add_utf8(buf, utf8.c_str(), (int)utf8.size(), 0, (int)utf8.size());
		hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
		hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
		hb_buffer_set_language(buf, hb_language_from_string("en", -1));

		hb_shape(font, buf, NULL, 0);

		uint32_t glyph_count = 0;
		hb_glyph_info_t *glyph_infos = hb_buffer_get_glyph_infos(buf, &glyph_count);
		hb_glyph_position_t *glyph_positions = hb_buffer_get_glyph_positions(buf, &glyph_count);

		glm::ivec2 cursor = glm::ivec2(0);
		for (uint32_t i = 0; i < glyph_count; ++i) {
			if (_indices) {
				_indices->emplace_back(glyph_infos[i].codepoint); //<-- actually a glyph index after shaping
			}
			if (_positions) {
				_positions->emplace_back((cursor.x + glyph_positions[i].x_offset) / 64.0f / float(OVERSAMPLE), (cursor.y + glyph_positions[i].y_offset) / 64.0f / float(OVERSAMPLE));
			}
			cursor.x += glyph_positions[i].x_advance;
			cursor.y += glyph_positions[i].y_advance;
			if (_cursors_after) {
				_cursors_after->emplace_back(cursor.x / 64.0f / float(OVERSAMPLE), cursor.y / 64.0f / float(OVERSAMPLE));
			}
		}
		hb_buffer_destroy(buf);
	}

};


Font::Font(std::string const &path) {
	internals.reset(new FontInternals(path));
}

Font::~Font() {
}

void Font::get_metrics(std::string const &text, float height, glm::vec2 *_advance, glm::vec2 *_bounds_min, glm::vec2 *_bounds_max) const {
	std::vector< FT_UInt > glyph_indices;
	std::vector< glm::vec2 > positions;
	std::vector< glm::vec2 > cursors_after;

	internals->shape_text(text, &glyph_indices, &positions, &cursors_after);

	if (glyph_indices.empty()) {
		if (_advance) {
			*_advance = glm::vec2(0.0f);
		}
		if (_bounds_min) {
			*_bounds_min = glm::vec2(0.0f);
		}
		if (_bounds_max) {
			*_bounds_max = glm::vec2(0.0f);
		}
		return;
	}

	float scale = height / float(RENDER_HEIGHT);

	if (_advance) {
		*_advance = scale * cursors_after.back();
	}

	if (_bounds_min || _bounds_max) {
		internals->upload_glyphs( glyph_indices );
		glm::vec2 min = glm::vec2(std::numeric_limits< float >::infinity());
		glm::vec2 max = glm::vec2(-std::numeric_limits< float >::infinity());
		for (uint32_t i = 0; i < glyph_indices.size(); ++i) {
			auto f = internals->rendered.find(glyph_indices[i]);
			if (f == internals->rendered.end()) continue; //WEIRD
			glm::vec2 at = scale * positions[i];
			Glyph const &glyph = f->second;

			min = glm::min(min, at + scale * glm::vec2(glyph.offset));
			max = glm::max(max, at + scale * glm::vec2(glyph.offset + glm::vec2(glyph.size)));
		}
		if (min.x > max.x) {
			min = max = glm::vec2(0.0f);
		}

		if (_bounds_min) {
			*_bounds_min = min;
		}
		if (_bounds_max) {
			*_bounds_max = max;
		}
	}
}

void Font::draw(glm::mat4 const &world_to_clip, std::string const &text, glm::vec2 const &anchor, float height, glm::u8vec4 const &tint ) const {
	draw(world_to_clip, text, anchor, height, glm::vec4(tint) / 255.0f);
}

void Font::draw(glm::mat4 const &world_to_clip, std::string const &text, glm::vec2 const &anchor, float height, glm::vec4 const &tint ) const {
	std::vector< FT_UInt > glyph_indices;
	std::vector< glm::vec2 > positions;
	std::vector< glm::vec2 > cursors_after;

	internals->shape_text(text, &glyph_indices, &positions, &cursors_after);
	internals->upload_glyphs( glyph_indices );

	/*
	//DEBUG: draw whole texture:
	attribs.emplace_back(anchor + glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 1.0f), tint);
	attribs.emplace_back(anchor + glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 0.0f), tint);
	attribs.emplace_back(anchor + glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 1.0f), tint);
	attribs.emplace_back(anchor + glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 0.0f), tint);
	*/

	/*OLD:
	std::vector< DrawStuff::Pos2f_Tex2f_Col4f > attribs;
	auto do_quad = [&](glm::vec2 const &pos_min, glm::vec2 const &pos_max, glm::vec2 const &tex_min, glm::vec2 const &tex_max) {
		if (!attribs.empty()) attribs.emplace_back(attribs.back());
		attribs.emplace_back(glm::vec2(pos_min.x, pos_max.y), glm::vec2(tex_min.x, tex_max.y), tint);
		if (attribs.size() != 1) attribs.emplace_back(attribs.back());
		attribs.emplace_back(glm::vec2(pos_min.x, pos_min.y), glm::vec2(tex_min.x, tex_min.y), tint);
		attribs.emplace_back(glm::vec2(pos_max.x, pos_max.y), glm::vec2(tex_max.x, tex_max.y), tint);
		attribs.emplace_back(glm::vec2(pos_max.x, pos_min.y), glm::vec2(tex_max.x, tex_min.y), tint);
	};
	*/

	std::vector< GLAttribBuffer< glm::vec2, glm::vec2 >::Vertex > attribs;
	auto do_quad = [&](glm::vec2 const &pos_min, glm::vec2 const &pos_max, glm::vec2 const &tex_min, glm::vec2 const &tex_max) {
		if (!attribs.empty()) attribs.emplace_back(attribs.back());
		attribs.emplace_back(glm::vec2(pos_min.x, pos_max.y), glm::vec2(tex_min.x, tex_max.y));
		if (attribs.size() != 1) attribs.emplace_back(attribs.back());
		attribs.emplace_back(glm::vec2(pos_min.x, pos_min.y), glm::vec2(tex_min.x, tex_min.y));
		attribs.emplace_back(glm::vec2(pos_max.x, pos_max.y), glm::vec2(tex_max.x, tex_max.y));
		attribs.emplace_back(glm::vec2(pos_max.x, pos_min.y), glm::vec2(tex_max.x, tex_min.y));
	};


	//Draw glyphs:

	float scale = height / float(RENDER_HEIGHT);
	for (uint32_t i = 0; i < glyph_indices.size(); ++i) {
		auto f = internals->rendered.find(glyph_indices[i]);
		if (f == internals->rendered.end()) continue; //WEIRD
		glm::vec2 at = anchor + scale * positions[i];
		Glyph const &glyph = f->second;

		//the various +/-0.5f here are to place borders of quad on texel centers:
		do_quad(
			at + scale * (glm::vec2(glyph.offset) + glm::vec2(0.5f)),
			at + scale * (glm::vec2(glyph.offset) + glm::vec2(glyph.size) - glm::vec2(0.5f)),
			(glm::vec2(glyph.packed_origin) + glm::vec2(0.5f)) / glm::vec2(internals->packed_size),
			(glm::vec2(glyph.packed_origin + glyph.size) - glm::vec2(0.5f)) / glm::vec2(internals->packed_size)
		);
	}

	text_buffer->set(attribs, GL_STREAM_DRAW);
	glUseProgram(distance_text_program->program);
	glUniformMatrix4fv(distance_text_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip));
	glUniform4fv(distance_text_program->TINT_vec4, 1, glm::value_ptr(tint));
	glBindVertexArray(text_buffer_for_distance_text_program->array);
	glBindTexture(GL_TEXTURE_2D, internals->packed_tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, text_buffer->count);
	//DrawStuff::draw(world_to_clip, GL_TRIANGLE_STRIP, attribs);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
	glUseProgram(0);
}




static std::unordered_map< std::string, std::unique_ptr< Font > > font_cache;

Font const &Font::get_shared(std::string const &path) {
	std::string norm_path = path;
	if (norm_path == "") {
		norm_path = data_path("fonts/Jost-Regular.ttf");
	} else {
		norm_path = data_path("fonts/" + path);
	}
	{ //see if font already is in the cache:
		auto f = font_cache.find(norm_path);
		if (f != font_cache.end()) return *f->second;
	}

	auto ret = font_cache.emplace(norm_path, new Font(norm_path));

	return *ret.first->second;
}
