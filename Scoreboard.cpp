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
const uint32_t FindWait = 20;

Scoreboard::Scoreboard(Server &server_, std::vector< std::string > const &maze_, std::vector< Player > const &players_) : maze(maze_), players(players_), server(server_) {
}
Scoreboard::~Scoreboard() {
}

bool Scoreboard::handle_event(SDL_Event const &, glm::uvec2 const &window_size) {
	return false;
}

void Scoreboard::update(float elapsed) {

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
					std::string secret = std::string(as_chars + 3, as_chars + 8);
					std::string andrewid = std::string(as_chars + 8, as_chars + 2 + length);

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
					if (role == 'g') {
						if (player.gatherer) {
							disconnect(player.gatherer, "Newer gatherer connected.");
						}
						info.state = ConnectionInfo::Playing;
						player.gatherer = connection;
						player.gatherer_state = Player::PendingView;
						player.gatherer_at = random_empty();
					} else if (role == 's') {
						if (player.seeker) {
							disconnect(player.seeker, "Newer seeker connected.");
						}
						info.state = ConnectionInfo::Playing;
						player.seeker = connection;
						player.seeker_state = Player::PendingView;
						player.seeker_at = random_empty();
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
					} else if (connection == info.player->seeker) {
						if (info.player->seeker_state != Player::WaitingMove) {
							disconnect(connection, "Don't send a 'M'ove unless you got a 'V'iew.");
							break;
						}
						if (found) {
							declare_found(info.player);
						} else if (direction != glm::ivec2(0)) {
							step(&info.player->seeker_at, direction);
						}
						info.player->gatherer_state = Player::PendingView;
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
		for (uint32_t row = 0; row < maze.size(); ++row) {
			glm::ivec2 px;
			px.y = maze_ul.y - int32_t(row);
			if (!(0 <= px.y && uint32_t(px.y) < display_size.y)) continue;

			for (uint32_t col = 0; col < maze[row].size(); ++col) {
				px.x = maze_ul.x + int32_t(col);
				if (!(0 <= px.x && uint32_t(px.x) < display_size.x)) continue;

				Cell &cell = display[px.y * display_size.x + px.x];
				cell.codepoint = maze[row][col];
				cell.fg = glm::u8vec3(0x88, 0x88, 0x88);
				if (maze[row][col] == ' ') {
					cell.bg = glm::u8vec3(0x44, 0x44, 0x44);
				} else {
					cell.bg = glm::u8vec3(0x00, 0x00, 0x00);
				}
				
			}
		}
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
				empty.emplace_back(x,y);
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

	if (player.gatherer && player.seeker) {
		if (player.gatherer_at == player.seeker_at) {
			player.finds += 1;
			disconnect(player.gatherer, "Good job, you were found by the seeker! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
			disconnect(player.seeker, "Good job, you were found the gatherer! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
		} else {
			disconnect(player.gatherer, "Unsuccessful find attempt! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
			disconnect(player.seeker, "Unsuccessful find attempt! Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
		}
	}
	if (player.gatherer && !player.seeker) {
		disconnect(player.gatherer, "Unsuccessful find attempt -- no seeker(?!). Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
	}
	if (!player.gatherer && player.seeker) {
		disconnect(player.seeker, "Unsuccessful find attempt -- no gatherer(?!). Reconnect after " + std::to_string(FindWait) + " seconds to play again.");
	}

	player.connect_wait = FindWait;
}

void Scoreboard::tell(Connection *connection, std::string const &message) {
	connection->send_buffer.emplace_back(uint8_t('T'));

	//note: truncated to 255 chars:
	size_t length = std::min< size_t >(255, message.size());
	connection->send_buffer.emplace_back(length);

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

