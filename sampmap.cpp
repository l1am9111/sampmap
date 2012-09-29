#include <cstdio>
#include <cstring>
#include <map>
#include <iostream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <libwebsockets.h>

#include <sampgdk/a_players.h>
#include <sampgdk/a_samp.h>
#include <sampgdk/core.h>
#include <sampgdk/plugin.h>

#define logprintf sampgdk_logprintf

//using namespace sampgdk;
typedef std::map<int, struct playerdata> PlayerType;
PlayerType players;
std::stringstream webbuffer (std::stringstream::in | std::stringstream::out);
int clients = 0;
#define TICKS 15
int ticks = 0;
struct libwebsocket_context *context;

static int callback_map(struct libwebsocket_context *context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len);

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

static int callback_map(struct libwebsocket_context *context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len);

struct libwebsocket_extension libwebsocket_internal_extensions[] = {
	{ /* terminator */
		NULL, NULL, 0
	}
};

static struct libwebsocket_protocols protocols[] = {
	{
		"map",
		callback_map,
		0,
	},
	{
		NULL, NULL, 0		/* End of list */
	}
};

void websocket_send(struct libwebsocket *wsi)
{
	int length = webbuffer.str().length();
	unsigned char *buf = new unsigned char[LWS_SEND_BUFFER_PRE_PADDING + length+1 + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	memcpy(p, webbuffer.str().c_str(), length);
	p[length] = '\0';
	libwebsocket_write(wsi, p, length+1, LWS_WRITE_TEXT);
	delete[] buf;
}

void websocket_send_to_all()
{
	int length = webbuffer.str().length();
	unsigned char *buf = new unsigned char[LWS_SEND_BUFFER_PRE_PADDING + length+1 + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	memcpy(p, webbuffer.str().c_str(), length);
	p[length] = '\0';
	libwebsockets_broadcast(&protocols[0], p, length+1);
	delete[] buf;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnGameModeInit()
{
	players.clear();
	return true;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerConnect(int playerid)
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
	websocket_send_to_all();
	webbuffer.str("");
	return true;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerDisconnect(int playerid, int reason)
{
	PlayerType::iterator player = players.find(playerid);
	players.erase(player);
	return true;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerSpawn(int playerid)
{
	float pos[3];
	GetPlayerPos(playerid, &pos[0], &pos[1], &pos[2]);
	players[playerid].x = static_cast<int>(pos[0]);
	players[playerid].y = static_cast<int>(pos[1]);
	players[playerid].spawned = true;
	webbuffer << CMD_EVENT;
	webbuffer << boost::format("%1%,%2%;") % EVENT_SPAWN % players[playerid].playerid;
	websocket_send_to_all();
	webbuffer.str("");
	return true;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerUpdate(int playerid)
{
	float pos[3];
	GetPlayerPos(playerid, &pos[0], &pos[1], &pos[2]);
	players[playerid].x = static_cast<int>(pos[0]);
	players[playerid].y = static_cast<int>(pos[1]);
	players[playerid].update = true;
	return true;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerStateChange(int playerid, int newstate, int oldstate)
{
	if(newstate == PLAYER_STATE_DRIVER || newstate == PLAYER_STATE_PASSENGER)
	{
		players[playerid].invehicle = true;
		webbuffer << CMD_EVENT;
		webbuffer << boost::format("%1%,%2%;") % EVENT_ENTERVEHICLE % players[playerid].playerid;
		websocket_send_to_all();
		webbuffer.str("");
	}
	else if(newstate == PLAYER_STATE_ONFOOT)
	{
		players[playerid].invehicle = false;
		webbuffer << CMD_EVENT;
		webbuffer << boost::format("%1%,%2%;") % EVENT_EXITVEHICLE % players[playerid].playerid;
		websocket_send_to_all();
		webbuffer.str("");
	}
	return true;
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerCommandText(int playerid, const char *cmdtext)
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

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
	return SUPPORTS_VERSION | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppPluginData)
{
	sampgdk_initialize(ppPluginData);

	context = libwebsocket_create_context(4444, "", protocols, libwebsocket_internal_extensions, NULL, NULL, -1, -1, 0);
	if (context == NULL)
	{
		fprintf(stderr, "Creating libwebsocket context failed\n");
	}
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
			websocket_send_to_all();
			webbuffer.str("");
			ticks = 0;
		}
		ticks++;
	}
	libwebsocket_service(context, 0);
}

static int callback_map(struct libwebsocket_context *context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{
	int n;
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 512 +
						  LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];

	switch (reason)
	{

	case LWS_CALLBACK_ESTABLISHED:
		clients++;
		webbuffer << CMD_PLAYERDATA;
		BOOST_FOREACH(PlayerType::value_type &player, players)
		{
			webbuffer << boost::format("%1%,%2%,%3%,%4%,%5%,%6%|") % player.second.playerid % player.second.invehicle % player.second.x % player.second.y % player.second.nick % player.second.spawned;
		}
		webbuffer << ";";
		websocket_send(wsi);
		webbuffer.str("");
		break;

	case LWS_CALLBACK_CLOSED:
		clients--;
		break;

	case LWS_CALLBACK_BROADCAST:
		libwebsocket_write(wsi, (unsigned char*)in, len, LWS_WRITE_TEXT);
		break;

	default:
		break;
	}

	return 0;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	sampgdk_finalize();
	return;
}
