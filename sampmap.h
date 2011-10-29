#ifndef HELLOWORLD_H
#define HELLOWORLD_H

#include <sampgdk/eventhandler.h>

class HelloWorld : public sampgdk::EventHandler {
public:
	HelloWorld();
	virtual ~HelloWorld();
	virtual void OnGameModeInit();
	virtual bool OnPlayerConnect(int playerid);
	virtual bool OnPlayerDisconnect(int playerid, int reason);
	virtual bool OnPlayerSpawn(int playerid);
	virtual bool OnPlayerUpdate(int playerid);
	virtual bool OnPlayerStateChange(int playerid, int newstate, int oldstate);
	virtual bool OnPlayerCommandText(int playerid, const std::string &cmdtext);
};

#endif