#pragma once

#include "Mode.hpp"

#include <string>
#include <vector>
#include <unordered_map>

struct Connection;
struct Server;

struct Player {
	std::string andrewid;
	std::string secret;

	std::string avatar;
	glm::u8vec3 color;
	glm::u8vec3 background;

	//active connections (if any):

	enum ConnectionState {
		Disconnected, //connection not active
		PendingView, //move was made, should send another view
		WaitingMove, //view was sent, wait for move in return
	};

	Connection *gatherer = nullptr;
	Connection *seeker = nullptr;
	ConnectionState gatherer_state = Disconnected;
	ConnectionState seeker_state = Disconnected;
	glm::ivec2 gatherer_at = glm::ivec2(-1);
	glm::ivec2 seeker_at = glm::ivec2(-1);
	
	void disconnect(Connection *); //call to reset gatherer and/or seeker state on disconnect

	//current score:
	uint32_t gems = 0;
	uint32_t finds = 0;

	//timeout after finds complete:
	float connect_wait = 0.0f;
};

struct ConnectionInfo {

	enum State {
		PendingHandshake, //just opened, waiting for handshake message (player is null)
		Playing, //post-handshake, is a gatherer or seeker (player is set)
		PendingClose, //an error happened, waiting to close (player is null)
	} state = PendingHandshake;

	Player *player = nullptr;

	float idle_time = 0.0f;
};

struct Scoreboard : Mode {
	Scoreboard(Server &server,
		std::vector< std::string > const &maze, //maze as text; ' ' is passage
		std::vector< Player > const &players //players
	);
	~Scoreboard();
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//helpers:
	glm::ivec2 random_empty() const; //random square with no walls in it
	bool step(glm::ivec2 *at, glm::ivec2 step); //step, if possible, in the maze
	void declare_found(Player *player); //declare that seeker and gatherer are in the same place

	//communication helpers:
	void tell(Connection *connection, std::string const &message); //send a 'T'ell message
	void disconnect(Connection *connection, std::string const &message); //disconnect a connection with a message

	std::vector< std::string > maze;
	std::vector< Player > players;

	Server &server;

	//track active connections:
	std::unordered_map< Connection *, ConnectionInfo > connections;
};
