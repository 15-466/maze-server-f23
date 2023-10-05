#include "Scoreboard.hpp"

#include "DrawStuff.hpp"
#include "Connection.hpp"
#include "Font.hpp"
#include "data_path.hpp"
#include "GL.hpp"

#include <iostream>
#include <random>

const uint32_t IdleTimeout = 60;
const uint32_t CloseTimeout = 20;
const uint32_t FindWait = 2; //6;

const float MoveWait = 0.25f;
const float GemWait = 10.0f;
const size_t GemMax = 20;

const float FindStarDecay = 1.4f;

Scoreboard::Scoreboard(Server &server_, std::vector< std::string > const &maze_, std::vector< Player > const &players_) : maze(maze_), players(players_), server(server_) {
}
Scoreboard::~Scoreboard() {
}

bool Scoreboard::handle_event(SDL_Event const &, glm::uvec2 const &window_size) {
	return false;
}

void Scoreboard::update(float elapsed) {
	timestamp += 1;

	server.poll([this](Connection *connection, Connection::Event evt) {
		if (evt == Connection::OnOpen) {
			//add to the connections list; ConnectionInfo()'s default state is pending handshake:
			connections.emplace(connection, ConnectionInfo());
			tell(connection, "Welcome to the maze server.");

		} else if (evt == Connection::OnClose) {
			//remove from connections list:
			auto f = connections.find(connection);
			if (f == connections.end()) {
				std::cerr << "Got an OnClose for a connection not in the list?!" << std::endl;
				return;
			}
			ConnectionInfo &info = f->second;
			if (info.player) {
				info.player->disconnect(connection);
				info.player = nullptr;
			}
			connections.erase(f);
		} else if (evt == Connection::OnRecv) {
			//will actually check all connections in a loop later
		}
	});


	for (auto &[connection, info] : connections) {
		//record elapsed time:
		info.idle_time += elapsed;

		//parse received data:
		while (info.state != ConnectionInfo::PendingClose) {
			//have a whole header yet?
			if (connection->recv_buffer.size() < 2) break;
			char type = char(connection->recv_buffer[0]);
			size_t length = size_t(connection->recv_buffer[1]);

			//have the whole message yet?
			if (connection->recv_buffer.size() < length + 2) break;

			//deal with message:
			if (info.state == ConnectionInfo::PendingHandshake) {
				if (type == 'H') {
					if (length < 1 + 6 + 1) {
						disconnect(connection, "Handshake message must contain role (1), secret (6), and andrewid (>=1), but length is " + std::to_string(length) + ".");
						break;
					}
					const char *as_chars = reinterpret_cast< const char * >(connection->recv_buffer.data());
					char role = as_chars[2];
					std::string secret = std::string(as_chars + 3, as_chars + 9);
					std::string andrewid = std::string(as_chars + 9, as_chars + 2 + length);

					auto p = std::find_if(players.begin(), players.end(), [&](Player const &player) {
						return player.andrewid == andrewid;
					});

					if (p == players.end()) {
						disconnect(connection, "Unknown andrewid '" + andrewid + "'.");
						break;
					}
					Player &player = *p;
					if (player.secret != secret) {
						disconnect(connection, "Incorrect secret '" + secret + "'.");
						break;
					}

					if (player.connect_wait != 0.0f) {
						disconnect(connection, "On cool-down; wait " + std::to_string(int32_t(std::ceil(player.connect_wait))) + " seconds more.");
						break;
					}

					if (player.timestamp == ~uint64_t(0)) {
						player.timestamp = timestamp;
					}
					if (role == 'g') {
						if (player.gatherer) {
							disconnect(player.gatherer, "Newer gatherer connected.");
						}
						info.state = ConnectionInfo::Playing;
						info.player = &player;
						player.gatherer = connection;
						player.gatherer_state = Player::PendingView;
						player.gatherer_at = random_empty();
						player.gatherer_cooldown = MoveWait;
					} else if (role == 's') {
						if (player.seeker) {
							disconnect(player.seeker, "Newer seeker connected.");
						}
						info.state = ConnectionInfo::Playing;
						info.player = &player;
						player.seeker = connection;
						player.seeker_state = Player::PendingView;
						player.seeker_at = random_empty();
						player.seeker_cooldown = MoveWait;
					} else {
						disconnect(connection, "Expecting role to be 'g'atherer or 's'eeker, got '" + std::string(&role, 1) + "' instead.");
						break;
					}
				} else {
					disconnect(connection, "Expecting 'H'andshake, got '" + std::string(&type, 1) + "' instead.");
					break;
				}
			} else if (info.state == ConnectionInfo::Playing) {
				assert(info.player);
				if (type == 'M') {
					if (length != 1) {
						disconnect(connection, "Expecting 'M'ove to have length 1, got " + std::to_string(length) + " instead.");
						break;
					}
					char move = char(connection->recv_buffer[2]);
					glm::ivec2 direction(0,0);
					bool found = false;
					if      (move == 'N') direction = glm::ivec2( 0,-1);
					else if (move == 'S') direction = glm::ivec2( 0, 1);
					else if (move == 'E') direction = glm::ivec2( 1, 0);
					else if (move == 'W') direction = glm::ivec2(-1, 0);
					else if (move == '.') direction = glm::ivec2( 0, 0);
					else if (move == 'f') found = true;
					else {
						disconnect(connection, "Expecting 'M'ove to be in {N, S, E, W, ., f}; got '" + std::string(&move, 1) + "'.");
						break;
					}

					if (connection == info.player->gatherer) {
						if (info.player->gatherer_state != Player::WaitingMove) {
							disconnect(connection, "Don't send a 'M'ove unless you got a 'V'iew.");
							break;
						}
						if (found) {
							disconnect(connection, "Gatherer cannot declare 'f'ound.");
							break;
						}
						if (direction != glm::ivec2(0)) {
							step(&info.player->gatherer_at, direction);
						}
						info.player->gatherer_state = Player::PendingView;
						info.player->gatherer_cooldown = MoveWait;
					} else if (connection == info.player->seeker) {
						if (info.player->seeker_state != Player::WaitingMove) {
							disconnect(connection, "Don't send a 'M'ove unless you got a 'V'iew.");
							break;
						}
						if (found) {
							declare_found(info.player);
							break;
						}
						if (direction != glm::ivec2(0)) {
							step(&info.player->seeker_at, direction);
						}
						info.player->seeker_state = Player::PendingView;
						info.player->seeker_cooldown = MoveWait;
					} else {
						std::cerr << "Connection marked 'Playing' but isn't gatherer or seeker?" << std::endl;
						disconnect(connection, "(Internal server error; sorry!)");
						break;
					}
				}
			} else {
				assert(0 && "unreachable case.");
			}


			//remove from the buffer:
			connection->recv_buffer.erase(connection->recv_buffer.begin(), connection->recv_buffer.begin() + 2 + length);

			info.idle_time = 0.0f;
		}

		//handle idle timeout:
		if (info.state != ConnectionInfo::PendingClose) {
			if (info.idle_time > IdleTimeout) {
				disconnect(connection, "Disconnecting because of no move for " + std::to_string(IdleTimeout) + " seconds. (You can move '.' to keep connection alive.)");
			}
		}
	}

	//trim connections that are pending close and have finished sending or timed out:
	for (auto ci = connections.begin(); ci != connections.end(); /* later */) {
		auto old = ci;
		++ci;

		Connection *connection = old->first;
		ConnectionInfo &info = old->second;

		if (info.state == ConnectionInfo::PendingClose) {
			if (connection->send_buffer.empty()) {
				//Great, got last data sent. A clean finish:
				connection->close();
				connections.erase(old);
			} else if (info.idle_time > CloseTimeout) {
				//Looks like other side is ignoring us and not taking data:
				connection->close();
				connections.erase(old);
			}
		}
	}

	//---------------------------------------------------
	//game logic (starting new moves etc)

	//collect gems:
	for (auto &player : players) {
		if (player.gatherer && player.gatherer_state != Player::Disconnected) {
			auto f = gems.find(player.gatherer_at);
			if (f != gems.end()) {
				uint32_t old_score = player.score();
				player.gems += 1;
				if (player.score() != old_score) player.timestamp = timestamp;
				gems.erase(f);

				//let them know:
				player.gatherer->send_buffer.emplace_back('G');
				player.gatherer->send_buffer.emplace_back(uint8_t(0));
			}
		}
	}
	
	//spawn a gem:
	gem_cooldown = std::max(0.0f, gem_cooldown - elapsed);
	if (gem_cooldown == 0.0f && gems.size() < GemMax) {
		gem_cooldown = GemWait;

		//find a random spot with no gems, walls, or gatherers:
		std::unordered_set< glm::ivec2 > gatherers;
		for (auto const &player : players) {
			if (player.gatherer && player.gatherer_state != Player::Disconnected) {
				gatherers.emplace(player.gatherer_at);
			}
		}

		static std::mt19937 mt(0x31415926);

		std::vector< glm::ivec2 > empty;

		for (uint32_t y = 0; y < maze.size(); ++y) {
			for (uint32_t x = 0; x < maze[y].size(); ++x) {
				if (maze[y][x] == ' ') {
					if (!gems.count(glm::ivec2(x,y))) {
						if (!gatherers.count(glm::ivec2(x,y))) {
							empty.emplace_back(x,y);
						}
					}
				}
			}
		}

		if (!empty.empty()) {
			gems.emplace(empty[mt() % empty.size()]);
		} else {
			std::cerr << "No gem spawning -- too full!" << std::endl;
		}
	}

	//cooldowns:
	for (auto &player : players) {
		player.connect_wait = std::max(0.0f, player.connect_wait - elapsed);
		player.gatherer_cooldown = std::max(0.0f, player.gatherer_cooldown - elapsed);
		player.seeker_cooldown = std::max(0.0f, player.seeker_cooldown - elapsed);

		//ask for new moves if got old moves:
		if (player.gatherer && player.gatherer_state == Player::PendingView && player.gatherer_cooldown == 0.0f) {
			send_view(player.gatherer, player.gatherer_at);
			player.gatherer_state = Player::WaitingMove;
		}
		if (player.seeker && player.seeker_state == Player::PendingView&& player.seeker_cooldown == 0.0f) {
			send_view(player.seeker, player.seeker_at);
			player.seeker_state = Player::WaitingMove;
		}
	}

	//find star animations:
	for (auto si = find_stars.begin(); si != find_stars.end(); /* later */) {
		auto old = si;
		++si;
		old->second = std::max(0.0f, old->second - elapsed / FindStarDecay);
		if (old->second == 0.0f) {
			find_stars.erase(old);
		}
	}
}

void Scoreboard::draw(glm::uvec2 const &drawable_size) {

	static Font const &font = Font::get_shared("Flexi_IBM_VGA_True.ttf");

	const glm::uvec2 display_size = glm::uvec2(84, 20);
	glm::vec2 char_size = glm::vec2(9.0f * 0.75f, 16.0f);

	glm::mat4 world_to_clip;

	{ //scaling from display to screen:
		glm::vec2 display_box = glm::vec2(display_size) * char_size;
		float scale = std::min(
			float(drawable_size.x) / display_box.x,
			float(drawable_size.y) / display_box.y
		);
		float sx = 2.0f / float(drawable_size.x) * scale;
		float sy = 2.0f / float(drawable_size.y) * scale;
		world_to_clip = glm::mat4(
			sx, 0.0f, 0.0f, 0.0f,
			0.0f, sy, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-0.5f * display_box.x * sx, (-0.5f * display_box.y + 4.0f) * sy, 0.0f, 1.0f
		);
	}

	//- - - - - - - -

	struct Cell {
		uint32_t codepoint = 0x256C;
		glm::u8vec3 fg = glm::u8vec3(0x22, 0x22, 0x22);
		glm::u8vec3 bg = glm::u8vec3(0x11, 0x11, 0x11);
	};

	std::vector< Cell > display(display_size.x * display_size.y);

	//- - - - - - - -

	{ //copy maze to display:
		glm::ivec2 maze_ul = glm::ivec2(1, display_size.y - 2);
		auto get_cell = [&](glm::ivec2 mz) -> Cell & {
			glm::ivec2 px = maze_ul + glm::ivec2(mz.x, -mz.y);
			static Cell temp;
			if (0 <= px.x && uint32_t(px.x) < display_size.x
			 && 0 <= px.y && uint32_t(px.y) < display_size.y) {
				return display[px.y * display_size.x + px.x];
			} else {
				return temp;
			}
		};
		for (uint32_t row = 0; row < maze.size(); ++row) {
			for (uint32_t col = 0; col < maze[row].size(); ++col) {
				Cell &cell = get_cell(glm::ivec2(col, row));
				cell.codepoint = maze[row][col];
				cell.fg = glm::u8vec3(0x88, 0x88, 0x88);
				if (maze[row][col] == ' ') {
					cell.bg = glm::u8vec3(0x44, 0x44, 0x44);
				} else {
					cell.bg = glm::u8vec3(0x00, 0x00, 0x00);
				}
				
			}
		}

		//find stars:
		for (auto const &fs : find_stars) {
			float amt = fs.second;
			float amt2 = std::min(1.0f, amt * 2.0f);
			Cell &cell = get_cell(fs.first);
			cell.codepoint = 0x263c;
			cell.fg = glm::u8vec3(uint8_t(0x88 * amt), uint8_t(0xbb * amt), uint8_t(0x22 * amt2)) + glm::u8vec3(0x44);
			cell.bg = glm::u8vec3(0x44);
		}


		//players in maze:
		for (auto const &player : players) {
			if (player.gatherer && player.gatherer_state != Player::Disconnected) {
				Cell &cell = get_cell(player.gatherer_at);
				cell.codepoint = player.avatar;
				cell.fg = player.color;
				cell.bg = player.background;
			}
		}

		//gems in maze:
		for (auto const &gem : gems) {
			Cell &cell = get_cell(gem);
			cell.codepoint = 0x2666;
			cell.bg = glm::u8vec3(0x22, 0x22, 0x44);
			cell.fg = glm::u8vec3(0x44, 0xaa, 0xff);
		}
	}

	{ //draw scoreboard on display:
		//01234567890123456789012345
		// rr A ss (gg ff) andrewid

		std::vector< Player const * > by_score;
		for (auto const &player : players) {
			by_score.emplace_back(&player);
		}
		std::sort(by_score.begin(), by_score.end(), [](Player const *a, Player const *b) {
			uint32_t sa = a->score();
			uint32_t sb = b->score();
			if (sa != sb) return sa > sb;
			else return a->timestamp < b->timestamp;
		});

		int32_t y = display_size.y - 1;
		int32_t x = display_size.x - 29;

		auto write = [&](uint32_t codepoint, glm::u8vec3 fg, glm::u8vec3 bg) {
			if (y < 0 || uint32_t(y) >= display_size.y) return;
			if (x < 0 || uint32_t(x) >= display_size.x) return;
			Cell &cell = display[y * display_size.x + x];
			cell.codepoint = codepoint;
			cell.fg = fg;
			cell.bg = bg;
			++x;
		};

		auto write_number = [&](uint32_t number, glm::u8vec3 fg, glm::u8vec3 bg) {
			number = std::min(99U, number);

			if (number >= 10) {
				write('0' + (number / 10), fg, bg);
			} else {
				write(' ', fg, bg);
			}
			write('0' + (number % 10), fg, bg);
		};

		auto write_string = [&](std::string string, uint32_t width, glm::u8vec3 fg, glm::u8vec3 bg) {
			string = string.substr(0,width); //truncate
			while (string.size() < width) string = string + ' '; //pad

			for (uint32_t i = 0; i < width; ++i) {
				write(string[i], fg, bg);
			}
		};

		const glm::u8vec3 border_fg(0x00);
		const glm::u8vec3 border_bg(0x22);

		//header:

		write(0x250C, border_fg, border_bg);
		for (uint32_t i = 0; i < 27; ++i) {
			write(0x2500, border_fg, border_bg);
		}
		write(0x2510, border_fg, border_bg);

		y -= 1;
		x = display_size.x - 29;

		write(0x2502, border_fg, border_bg);

		write(' ', glm::u8vec3(0xff), glm::u8vec3(0x22));
		write('#', glm::u8vec3(0xff), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0xff), glm::u8vec3(0x22));
		write('A', glm::u8vec3(0xaa), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0xff), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0xaa), glm::u8vec3(0x22));
		write('$', glm::u8vec3(0xaa), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0xff), glm::u8vec3(0x22));
		write('(', glm::u8vec3(0x88), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0xaa), glm::u8vec3(0x22));
		write(0x2666, glm::u8vec3(0x44, 0xaa, 0xff), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0x88), glm::u8vec3(0x22));
		write(' ', glm::u8vec3(0xaa), glm::u8vec3(0x22));
		write(0x263c, glm::u8vec3(0xbb, 0xee, 0x44), glm::u8vec3(0x22));
		write(')', glm::u8vec3(0x88), glm::u8vec3(0x22));

		write(' ', glm::u8vec3(0xff), glm::u8vec3(0x22));

		write_string("who", 8, glm::u8vec3(0xaa), glm::u8vec3(0x22));

		write(' ', glm::u8vec3(0xff), glm::u8vec3(0x22));

		write('g', glm::u8vec3(0xaa), glm::u8vec3(0x22));
		write('s', glm::u8vec3(0xaa), glm::u8vec3(0x22));

		write(0x2502, border_fg, border_bg);


		//players:
		for (uint32_t row = 0; row < std::min(display_size.y, uint32_t(by_score.size())); ++row) {
			y -= 1;
			x = display_size.x - 29;

			auto const &player = *by_score[row];

			write(0x2502, border_fg, border_bg);

			//rank:
			write_number(row+1, glm::u8vec3(0xff), glm::u8vec3(0x00));
			write(' ', glm::u8vec3(0xff), glm::u8vec3(0x00));

			//avatar:
			write(player.avatar, player.color, player.background);
			write(' ', glm::u8vec3(0xff), glm::u8vec3(0x00));

			//score:
			write_number(player.score(), glm::u8vec3(0xff), glm::u8vec3(0x00));
			write(' ', glm::u8vec3(0xff), glm::u8vec3(0x00));

			write('(', glm::u8vec3(0x88), glm::u8vec3(0x00));
			write_number(player.gems, glm::u8vec3(0xaa), glm::u8vec3(0x00));
			write(' ', glm::u8vec3(0x88), glm::u8vec3(0x00));
			write_number(player.finds, glm::u8vec3(0xaa), glm::u8vec3(0x00));
			write(')', glm::u8vec3(0x88), glm::u8vec3(0x00));


			write(' ', glm::u8vec3(0xff), glm::u8vec3(0x00));

			write_string(player.andrewid, 8, player.color, player.background);

			write(' ', glm::u8vec3(0xff), glm::u8vec3(0x00));

			if (player.connect_wait != 0.0f) {
				write_number(uint32_t(std::ceil(player.connect_wait)), glm::u8vec3(0xaa), glm::u8vec3(0x00));
			} else {
				if (player.gatherer_state == Player::Disconnected) {
					write(0x2219, glm::u8vec3(0xaa), glm::u8vec3(0x00));
				} else {
					write(0x2713, glm::u8vec3(0xff), glm::u8vec3(0x00));
				}

				if (player.seeker_state == Player::Disconnected) {
					write(0x2219, glm::u8vec3(0xaa), glm::u8vec3(0x00));
				} else {
					write(0x2713, glm::u8vec3(0xff), glm::u8vec3(0x00));
				}
			}

			write(0x2502, border_fg, border_bg);
		}

		y -= 1;
		x = display_size.x - 29;

		write(0x2514, border_fg, border_bg);
		for (uint32_t i = 0; i < 27; ++i) {
			write(0x2500, border_fg, border_bg);
		}
		write(0x2518, border_fg, border_bg);

	}

	//- - - - - - - -

	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	std::vector< DrawStuff::Pos2f_Col4ub > bg_attribs;
	std::vector< DrawStuff::Pos2f_Col4ub > fg_attribs;

	for (uint32_t row = 0; row < display_size.y; ++row) {
		std::string encoded;
		for (uint32_t col = 0; col < display_size.x; ++col) {
			Cell const &cell = display[row * display_size.x + col];
			uint32_t codepoint = cell.codepoint;

			glm::u8vec4 bg_color = glm::u8vec4(cell.bg, 0xff);
			glm::u8vec4 fg_color = glm::u8vec4(cell.fg, 0xff);

			//UTF8 encode from sejp (https://github.com/ixchow/sejp)
			if (codepoint <= 0x007f) {
				encoded += char(codepoint);
			} else if (codepoint <= 0x07ff) {
				encoded += char(0xc0 | (codepoint >> 6));
				encoded += char(0x80 | (codepoint & 0x3f));
			} else if (codepoint <= 0xffff) {
				encoded += char(0xe0 | (codepoint >> 12));
				encoded += char(0x80 | ((codepoint >> 6) & 0x3f));
				encoded += char(0x80 | (codepoint & 0x3f));
			} else { assert(codepoint <= 0x10ffff);
				encoded += char(0xf0 | (codepoint >> 18));
				encoded += char(0x80 | ((codepoint >> 12) & 0x3f));
				encoded += char(0x80 | ((codepoint >> 6) & 0x3f));
				encoded += char(0x80 | (codepoint & 0x3f));
			}

			glm::vec2 min = char_size * glm::vec2(col, row) - glm::vec2(0.0f,4.0f);
			glm::vec2 max = char_size * glm::vec2(col+1, row+1) - glm::vec2(0.0f, 4.0f);

			fg_attribs.emplace_back(glm::vec2(min.x, min.y), fg_color);
			fg_attribs.emplace_back(glm::vec2(max.x, min.y), fg_color);
			fg_attribs.emplace_back(glm::vec2(max.x, max.y), fg_color);
			fg_attribs.emplace_back(glm::vec2(min.x, min.y), fg_color);
			fg_attribs.emplace_back(glm::vec2(max.x, max.y), fg_color);
			fg_attribs.emplace_back(glm::vec2(min.x, max.y), fg_color);

			bg_attribs.emplace_back(glm::vec2(min.x, min.y), bg_color);
			bg_attribs.emplace_back(glm::vec2(max.x, min.y), bg_color);
			bg_attribs.emplace_back(glm::vec2(max.x, max.y), bg_color);
			bg_attribs.emplace_back(glm::vec2(min.x, min.y), bg_color);
			bg_attribs.emplace_back(glm::vec2(max.x, max.y), bg_color);
			bg_attribs.emplace_back(glm::vec2(min.x, max.y), bg_color);
			
		}
		font.draw(world_to_clip, encoded, char_size * glm::vec2(0, row), char_size.y, glm::u8vec4(0xff));
	}

	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	DrawStuff::draw(world_to_clip, GL_TRIANGLES, fg_attribs);


	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
	DrawStuff::draw(world_to_clip, GL_TRIANGLES, bg_attribs);

}

glm::ivec2 Scoreboard::random_empty() const {
	static std::mt19937 mt(0xfeedf00d);

	std::vector< glm::ivec2 > empty;

	for (uint32_t y = 0; y < maze.size(); ++y) {
		for (uint32_t x = 0; x < maze[y].size(); ++x) {
			if (maze[y][x] == ' ') {
				if (!gems.count(glm::ivec2(x,y))) {
					empty.emplace_back(x,y);
				}
			}
		}
	}

	if (empty.empty()) {
		std::cerr << "PROBLEM: no empty cells." << std::endl;
		return glm::ivec2(0,0);
	}

	return empty[mt() % empty.size()];
}

bool Scoreboard::step(glm::ivec2 *at_, glm::ivec2 step) {
	assert(at_);
	glm::ivec2 &at = *at_;

	glm::ivec2 next = at + step;

	if (next.y >= 0 && size_t(next.y) < maze.size()) {
		if (next.x >= 0 && size_t(next.x) < maze[next.y].size()) {
			if (maze[next.y][next.x] == ' ') {
				at = next;
				return true;
			}
		}
	}
	return false;
}

void Scoreboard::declare_found(Player *player_) {
	assert(player_);
	Player &player = *player_;

	if (player.gatherer) find_stars[player.gatherer_at] = 1.0f;
	if (player.seeker) find_stars[player.seeker_at] = 1.0f;

	if (player.gatherer && player.seeker) {
		if (player.gatherer_at == player.seeker_at) {
			uint32_t old_score = player.score();
			player.finds += 1;
			if (old_score != player.score()) player.timestamp = timestamp;
			disconnect(player.gatherer, "Good job, you were found by the seeker! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
			disconnect(player.seeker, "Good job, you were found the gatherer! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
		} else {
			disconnect(player.gatherer, "Unsuccessful find attempt! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
			disconnect(player.seeker, "Unsuccessful find attempt! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
		}
	} else if (player.gatherer && !player.seeker) {
		disconnect(player.gatherer, "Unsuccessful find attempt -- no seeker(?!). Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
	} else if (!player.gatherer && player.seeker) {
		disconnect(player.seeker, "Unsuccessful find attempt -- no gatherer(?!). Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
	}

	player.connect_wait = FindWait;
}

void Scoreboard::tell(Connection *connection, std::string const &message) {
	connection->send_buffer.emplace_back(uint8_t('T'));

	//note: truncated to 255 chars:
	size_t length = std::min< size_t >(255, message.size());
	connection->send_buffer.emplace_back(uint8_t(length));

	const char *as_chars = message.c_str();
	connection->send_buffer.insert(connection->send_buffer.end(),
		reinterpret_cast< uint8_t const * >(as_chars),
		reinterpret_cast< uint8_t const * >(as_chars) + length
	);
}

void Player::disconnect(Connection *connection) {
	if (gatherer == connection) {
		gatherer = nullptr;
		gatherer_state = Disconnected;
		gatherer_at = glm::ivec2(-1);
	}
	if (seeker == connection) {
		seeker = nullptr;
		seeker_state = Disconnected;
		seeker_at = glm::ivec2(-1);
	}
}

void Scoreboard::disconnect(Connection *connection, std::string const &message) {
	auto f = connections.find(connection);
	if (f == connections.end()) {
		std::cerr << "Trying to disconnect a connection that isn't in the list?" << std::endl;
		return;
	}
	ConnectionInfo &info = f->second;

	if (info.state == ConnectionInfo::PendingClose) {
		std::cerr << "Trying to disconnect a connection already closed?" << std::endl;
		return;
	}

	if (info.player) {
		info.player->disconnect(connection);
		info.player = nullptr;
	}

	tell(connection, message);

	info.state = ConnectionInfo::PendingClose;
	info.idle_time = 0.0f;
}

void Scoreboard::send_view(Connection *connection, glm::ivec2 at) {
	auto lookup = [this](glm::ivec2 pos) {
		if (0 <= pos.y && size_t(pos.y) < maze.size()) {
			if (0 <= pos.x && size_t(pos.x) < maze[pos.y].size()) {
				return maze[pos.y][pos.x];
			}
		}
		return '*';
	};

	connection->send_buffer.emplace_back(uint8_t('V'));
	connection->send_buffer.emplace_back(9);
	for (int32_t dy = -1; dy <= 1; dy += 1) {
		for (int32_t dx = -1; dx <= 1; dx += 1) {
			connection->send_buffer.emplace_back(lookup(at + glm::ivec2(dx,dy)));
		}
	}
}
