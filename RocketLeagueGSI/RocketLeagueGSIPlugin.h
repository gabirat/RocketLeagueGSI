#pragma once
#pragma comment( lib, "bakkesmod.lib" )
#include "Simple-Web-Server-master/server_http.hpp"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <nlohmann/json.hpp>


using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

class RocketLeagueGSIPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
public:
	virtual void onLoad();
	virtual void onUnload();
	void startGSI(std::vector<std::string> events);
	void stopGSI(std::vector<std::string> events);
	void update();
	bool isRunning();
	void testGSI(std::vector<std::string> events);
	void onGoalScored(PlayerControllerWrapper caller, void* params, std::string eventName);
	float time;
	std::string gameInfoToString();
	nlohmann::json gameInfo;
	nlohmann::json lastGoal;
private:
	bool running;
	HttpServer server;
};

