#include "Scoreboard.hpp"

#include "Connection.hpp"
#include "Mode.hpp"
#include "Load.hpp"
#include "GL.hpp"
#include "load_save_png.hpp"

#include <SDL.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <fstream>
#include <regex>

#ifdef _WIN32
extern "C" { uint32_t GetACP(); }
#endif
int main(int argc, char **argv) {
#ifdef _WIN32
	{ //when compiled on windows, check that code page is forced to utf-8 (makes file loading/saving work right):
		//see: https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
		uint32_t code_page = GetACP();
		if (code_page == 65001) {
			std::cout << "Code page is properly set to UTF-8." << std::endl;
		} else {
			std::cout << "WARNING: code page is set to " << code_page << " instead of 65001 (UTF-8). Some file handling functions may fail." << std::endl;
		}
	}

	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif
	//------------ command line arguments ------------
	if (argc != 4) {
		std::cerr << "Usage:\n\t./maze-server <port> <maze.txt> <players.txt>" << std::endl;
		return 1;
	}

	//--------- load maze ---------------------
	std::vector< std::string > maze;
	{
		std::ifstream text(argv[2], std::ios::binary);
		if (!text) {
			std::cerr << "Failed to open '" << argv[2] << "' when reading maze list." << std::endl;
			return 1;
		}
		std::string line;
		while (std::getline(text, line)) {
			maze.emplace_back(line);
		}

		std::cout << "Maze:\n";
		for (auto const &line : maze) {
			std::cout << "> " << line << '\n';
		}
		std::cout.flush();
	}
	//--------- parse client list -------------
	std::vector< Player > players;
	{
		std::ifstream text(argv[3], std::ios::binary);
		if (!text) {
			std::cerr << "Failed to open '" << argv[3] << "' when reading players list." << std::endl;
			return 1;
		}
		std::string line;
		while (std::getline(text, line)) {
			std::vector< std::string > split;
			split.emplace_back();
			for (char c : line) {
				if (c == ' ') {
					if (!split.back().empty()) {
						split.emplace_back();
					}
				} else {
					split.back() += c;
				}
			}
			if (split.back().empty()) split.pop_back();

			if (split.size() != 4) {
				std::cerr << "Expecting 4 tokens on line, got " << split.size() << ".\n line: '" << line << "'" << std::endl;
				return 1;
			}

			std::string andrewid = split[0];
			std::string secret = split[1];
			std::string avatar = split[2];
			std::string color = split[3];

			if (secret.size() != 6) {
				std::cerr << "All secrets should be size 6, but '" << secret << "' is size " << secret.size() << "!" << std::endl;
				return 1;
			}

			//TODO: UTF8, bruv!
			if (avatar.size() != 1) {
				std::cerr << "All avatars should be size 1, but '" << avatar << "' is size " << avatar.size() << "!" << std::endl;
				return 1;
			}

			static std::regex hex_color("#[0-9a-fA-F]{6}");
			std::smatch m;
			if (!std::regex_match(color, m, hex_color)) {
				std::cerr << "All colors should be #RRGGBB hex tuples, but '" << color << "' is not!" << std::endl;
			}

			Player player;
			player.andrewid = andrewid;
			player.secret = secret;
			player.avatar = avatar;
			player.color.r = uint8_t(std::stoi(color.substr(1,2), nullptr, 16));
			player.color.g = uint8_t(std::stoi(color.substr(3,2), nullptr, 16));
			player.color.b = uint8_t(std::stoi(color.substr(5,2), nullptr, 16));

			if (glm::dot(glm::vec3(player.color), glm::vec3(1.0f)) > 255.0f + 127.0f) {
				player.background = glm::u8vec3(0x00, 0x00, 0x00);
			} else {
				player.background = glm::u8vec3(0xff, 0xff, 0xff);
			}


		}
	}

	//------------ set up server --------------
	Server server(argv[1]);

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		"Maze Challenge",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1280, 720, //TODO: modify window size if you'd like
		SDL_WINDOW_OPENGL
		| SDL_WINDOW_RESIZABLE //uncomment to allow resizing
		| SDL_WINDOW_ALLOW_HIGHDPI //uncomment for full resolution on high-DPI screens
	);

	//prevent exceedingly tiny windows when resizing:
	SDL_SetWindowMinimumSize(window,100,100);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	//On windows, load OpenGL entrypoints: (does nothing on other platforms)
	init_GL();

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ load assets --------------
	call_load_functions();

	//------------ create game mode + make current --------------
	Mode::set_current(std::make_shared< Scoreboard >(server, maze, players));

	//------------ main loop ------------

	//this inline function will be called whenever the window is resized,
	// and will update the window_size and drawable_size variables:
	glm::uvec2 window_size; //size of window (layout pixels)
	glm::uvec2 drawable_size; //size of drawable (physical pixels)
	//On non-highDPI displays, window_size will always equal drawable_size.
	auto on_resize = [&](){
		int w,h;
		SDL_GetWindowSize(window, &w, &h);
		window_size = glm::uvec2(w, h);
		SDL_GL_GetDrawableSize(window, &w, &h);
		drawable_size = glm::uvec2(w, h);
		glViewport(0, 0, drawable_size.x, drawable_size.y);
	};
	on_resize();

	//This will loop until the current mode is set to null:
	while (Mode::current) {
		//every pass through the game loop creates one frame of output
		//  by performing three steps:

		{ //(1) process any events that are pending
			static SDL_Event evt;
			while (SDL_PollEvent(&evt) == 1) {
				//handle resizing:
				if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					on_resize();
				}
				//handle input:
				if (Mode::current && Mode::current->handle_event(evt, window_size)) {
					// mode handled it; great
				} else if (evt.type == SDL_QUIT) {
					Mode::set_current(nullptr);
					break;
				} else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_PRINTSCREEN) {
					// --- screenshot key ---
					std::string filename = "screenshot.png";
					std::cout << "Saving screenshot to '" << filename << "'." << std::endl;
					glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
					glReadBuffer(GL_FRONT);
					int w,h;
					SDL_GL_GetDrawableSize(window, &w, &h);
					std::vector< glm::u8vec4 > data(w*h);
					glReadPixels(0,0,w,h, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
					for (auto &px : data) {
						px.a = 0xff;
					}
					save_png(filename, glm::uvec2(w,h), data.data(), LowerLeftOrigin);
				}
			}
			if (!Mode::current) break;
		}

		{ //(2) call the current mode's "update" function to deal with elapsed time:
			auto current_time = std::chrono::high_resolution_clock::now();
			static auto previous_time = current_time;
			float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
			previous_time = current_time;

			//if frames are taking a very long time to process,
			//lag to avoid spiral of death:
			elapsed = std::min(0.1f, elapsed);

			Mode::current->update(elapsed);
			if (!Mode::current) break;
		}

		{ //(3) call the current mode's "draw" function to produce output:
		
			Mode::current->draw(drawable_size);
		}

		//Wait until the recently-drawn frame is shown before doing it all again:
		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}