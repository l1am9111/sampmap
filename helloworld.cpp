#include "helloworld.h"

#include <sampgdk/amxplugin.h>
#include <sampgdk/players.h>
#include <sampgdk/samp.h>
#include <sampgdk/wrapper.h>

#include <cstdio>
#include <cstring>
#include "handler.h"
#include <websocketpp.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <map>
#include <iostream>

static logprintf_t logprintf;

using boost::asio::ip::tcp;
using handler::web_handler;
using namespace sampgdk;

boost::asio::io_service io_service;
handler::echo_server_handler_ptr samp_handler(new handler::web_handler());
static HelloWorld theGameMode;
typedef std::map<int, struct playerdata> PlayerType;
PlayerType players;
std::stringstream webbuffer (std::stringstream::in | std::stringstream::out);
int clients = 0;
#define TICKS 15
int ticks = 0;

struct playerdata
{
	char nick[MAX_PLAYER_NAME];
	int playerid;
	bool invehicle;
	int x;
	int y;
	bool spawned;
	bool update;
};

enum
{
	CMD_UPDATE,
	CMD_PLAYERDATA,
	CMD_REMOVEPLAYER,
	CMD_EVENT
};

enum
{
	EVENT_ENTERVEHICLE,
	EVENT_EXITVEHICLE,
	EVENT_SPAWN
};

HelloWorld::HelloWorld() {
	// Register our gamemode in order to catch events - if we don't do this
	// somewhere none of the HelloWorld callbacks will be ever called.
	this->Register();
}

HelloWorld::~HelloWorld() {}

void HelloWorld::OnGameModeInit()
{
	players.clear();
}

bool HelloWorld::OnPlayerConnect(int playerid)
{
	struct playerdata player;
	char nick[MAX_PLAYER_NAME];
	GetPlayerName(playerid, nick);
	player.invehicle = 0;
	strcpy(player.nick, nick);
	player.playerid = playerid;
	player.x = 0;
	player.y = 0;
	players[playerid] = player;

	webbuffer << CMD_PLAYERDATA;
	webbuffer << boost::format("%1%,%2%,%3%,%4%,%5%,%6%;") % player.playerid % player.invehicle % player.x % player.y % player.nick % player.spawned;
	samp_handler->send_to_all(webbuffer.str());
	webbuffer.str("");
	return true;
}

bool HelloWorld::OnPlayerDisconnect(int playerid, int reason)
{
	PlayerType::iterator player = players.find(playerid);
	players.erase(player);
	return true;
}

bool HelloWorld::OnPlayerSpawn(int playerid)
{
	float pos[3];
	GetPlayerPos(playerid, pos[0], pos[1], pos[2]);
	players[playerid].x = static_cast<int>(pos[0]);
	players[playerid].y = static_cast<int>(pos[1]);
	players[playerid].spawned = true;
	webbuffer << CMD_EVENT;
	webbuffer << boost::format("%1%,%2%;") % EVENT_SPAWN % players[playerid].playerid;
	samp_handler->send_to_all(webbuffer.str());
	webbuffer.str("");
	return true;
}

bool HelloWorld::OnPlayerUpdate(int playerid)
{
	float pos[3];
	GetPlayerPos(playerid, pos[0], pos[1], pos[2]);
	players[playerid].x = static_cast<int>(pos[0]);
	players[playerid].y = static_cast<int>(pos[1]);
	players[playerid].update = true;
	return true;
}

bool HelloWorld::OnPlayerStateChange(int playerid, int newstate, int oldstate)
{
	if(newstate == PLAYER_STATE_DRIVER || newstate == PLAYER_STATE_PASSENGER)
	{
		players[playerid].invehicle = true;
		webbuffer << CMD_EVENT;
		webbuffer << boost::format("%1%,%2%;") % EVENT_ENTERVEHICLE % players[playerid].playerid;
		samp_handler->send_to_all(webbuffer.str());
		webbuffer.str("");
	}
	else if(newstate == PLAYER_STATE_ONFOOT)
	{
		players[playerid].invehicle = false;
		webbuffer << CMD_EVENT;
		webbuffer << boost::format("%1%,%2%;") % EVENT_EXITVEHICLE % players[playerid].playerid;
		samp_handler->send_to_all(webbuffer.str());
		webbuffer.str("");
	}
	return true;
}

bool HelloWorld::OnPlayerCommandText(int playerid, const std::string &cmdtext)
{
	if (cmdtext == "/players")
	{
		char message[128];
		BOOST_FOREACH(PlayerType::value_type &player, players)
		{
			std::sprintf(message, "%s[%d] is at %d %d", player.second.nick, player.second.playerid, player.second.x, player.second.y);
			SendClientMessage(playerid, 0xFF000000, message);
		}
		return true;
	}
	return false;
}

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
	return SUPPORTS_VERSION | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppPluginData) {
	logprintf = (logprintf_t)ppPluginData[PLUGIN_DATA_LOGPRINTF];
	// Initialize the wrapper - this always should be done here.
	Wrapper::GetInstance().Initialize(ppPluginData);
	// Do not call any natives here - they are not yet prepared for use at this stage.

	tcp::endpoint endpoint(tcp::v6(), 4444);

	websocketpp::server_ptr server(new websocketpp::server(io_service, endpoint, samp_handler));
	server->add_host("127.0.0.1:4444");
	server->set_max_message_size(0xFF);
	server->start_accept();
	server->set_alog_level(websocketpp::ALOG_OFF);
	server->set_elog_level(websocketpp::LOG_OFF);
	return true;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick()
{
	if(clients > 0)
	{
		if(ticks == TICKS)
		{
			webbuffer << CMD_UPDATE;
			BOOST_FOREACH(PlayerType::value_type &player, players)
			{
				if(player.second.update)
				{
					webbuffer << boost::format("%1%,%2%,%3%|") % player.second.playerid % player.second.x % player.second.y;
					player.second.update = false;
				}
			}
			webbuffer << ";";
			samp_handler->send_to_all(webbuffer.str());
			webbuffer.str("");
			ticks = 0;
		}
		ticks++;
	}
	io_service.poll();
}

void web_handler::validate(websocketpp::session_ptr client) {}

void web_handler::on_open(session_ptr client)
{
	connections.push_back(client);
	clients++;

	webbuffer << CMD_PLAYERDATA;
	BOOST_FOREACH(PlayerType::value_type &player, players)
	{
		webbuffer << boost::format("%1%,%2%,%3%,%4%,%5%,%6%|") % player.second.playerid % player.second.invehicle % player.second.x % player.second.y % player.second.nick % player.second.spawned;
	}
	webbuffer << ";";
	client->send(webbuffer.str());
	webbuffer.str("");
}

void web_handler::on_close(session_ptr client)
{
	std::list<session_ptr>::iterator it = std::find_if(connections.begin(), connections.end(), std::bind2nd( std::equal_to<session_ptr>(), client));
	if(it == connections.end()) return;
	connections.erase(it);
	clients--;
}

void web_handler::on_message(websocketpp::session_ptr client, const std::string &msg)
{
}

void web_handler::on_message(websocketpp::session_ptr client, const std::vector<unsigned char> &data)
{
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	io_service.stop();
	return;
}
