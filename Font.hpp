#pragma once

//Adapted from Font.hpp in from https://github.com/ixchow/preshack2

#include <glm/glm.hpp>

#include <string>
#include <memory>

/*
 * A way-too-fancy replacement for the venerable TexFont-based util/Graphics/Font
 *
 */

struct FontInternals;

struct Font {
	Font(std::string const &path);
	~Font();

	//get bounds and overall cursor advance for text:
	void get_metrics(std::string const &text, float height, glm::vec2 *advance, glm::vec2 *bounds_min, glm::vec2 *bounds_max) const;

	//draw some text, starting at anchor:
	void draw(glm::mat4 const &world_to_clip, std::string const &text, glm::vec2 const &anchor, float height, glm::u8vec4 const &tint = glm::u8vec4(0xff) ) const;
	void draw(glm::mat4 const &world_to_clip, std::string const &text, glm::vec2 const &anchor, float height, glm::vec4 const &tint) const;

	//uses a glyph cache of some sort...
	std::unique_ptr< FontInternals > internals;

	//a cache of fonts is maintained:
	// (passing "" returns the default font)
	static Font const &get_shared(std::string const &path = "");
};
