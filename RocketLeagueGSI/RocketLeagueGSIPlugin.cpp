#include "RocketLeagueGSIPlugin.h"

BAKKESMOD_PLUGIN(RocketLeagueGSIPlugin, "RocketLeagueGSI", "0.1", PLUGINTYPE_FREEPLAY)

#define LOG(x)
#ifdef RATAJ_DEBUG
	#define LOG(x) cvarManager->log(x)
#endif

	

void RocketLeagueGSIPlugin::onLoad() {
	cvarManager->registerNotifier("gsi_start", std::bind(&RocketLeagueGSIPlugin::startGSI, this, std::placeholders::_1), "Starts Gamestate Integration", PERMISSION_ALL);
	cvarManager->registerNotifier("gsi_stop", std::bind(&RocketLeagueGSIPlugin::stopGSI, this, std::placeholders::_1), "Stops Gamestate Integration", PERMISSION_ALL);
	cvarManager->registerNotifier("gsi_test", std::bind(&RocketLeagueGSIPlugin::testGSI, this, std::placeholders::_1), "xxx", PERMISSION_ALL);
	//gameWrapper->HookEvent("Function TAGame.PRI_TA.OnScoredGoal", std::bind(&RocketLeagueGSIPlugin::onGoalScored, this, std::placeholders::_1));
	cvarManager->registerCvar("gsi_port", "1337", "Port for the server to listen on");
	cvarManager->registerCvar("gsi_address", "127.0.0.1", "Address for the server to listen on");
	//gameWrapper->HookEventWithCaller("Function TAGame.GameEvent_Soccar_TA.EventGoalScored", std::bind(&RocketLeagueGSIPlugin::onGoalScored, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	//DOESN WORK ON OWNGOALS gameWrapper->HookEventWithCaller<PlayerControllerWrapper>("Function TAGame.GameEvent_Soccar_TA.EventPlayerScored", std::bind(&RocketLeagueGSIPlugin::onGoalScored, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	gameWrapper->HookEventWithCaller<PlayerControllerWrapper>("Function TAGame.PlayerController_TA.NotifyGoalScored", std::bind(&RocketLeagueGSIPlugin::onGoalScored, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	//Params to callbacks can be found in files ex. NotifyGoalScored is a method in PlayerControllerWrapper and has arguments
}
void RocketLeagueGSIPlugin::onUnload() {
	if (running) {
		server.stop();
		running = false;
	}
}

void RocketLeagueGSIPlugin::startGSI(std::vector<std::string> events) {
	if (!running) {
		unsigned int port = cvarManager->getCvar("gsi_port").getIntValue();
		std::string address = cvarManager->getCvar("gsi_address").getStringValue();
		server.config.address = address;
		server.config.port = port;
		running = true;

		lastGoal["noGoalsYet"] = true;

		server.resource["^/rlgsi$"]["GET"] = [this](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
			auto content = request->content.string();
			response->write(gameInfoToString());
		};

		server.on_error = [this](shared_ptr<HttpServer::Request> request, const SimpleWeb::error_code & ec) {
			LOG(ec.message());
		};

		std::thread serverThread([&server = server]() {
			server.start();
		});
		serverThread.detach();

		std::thread updaterThread(&RocketLeagueGSIPlugin::update, this);
		updaterThread.detach();
	}
}

void RocketLeagueGSIPlugin::stopGSI(std::vector<std::string> events) {
	server.stop();
	running = false;
}

void RocketLeagueGSIPlugin::onGoalScored(PlayerControllerWrapper caller, void* params, std::string eventName) {
	int scoredOnTeamIndex = *(int*)params;
	int teamThatScored = scoredOnTeamIndex ? 0 : 1; //returns the other team
	lastGoal["noGoalsYet"] = false;
	lastGoal["scoringTeamIndex"] = teamThatScored;
	lastGoal["scoredOnTeamIndex"] = scoredOnTeamIndex;
	lastGoal["scoreTimestapm"] = gameInfo["matchInfo"]["secondsElapsed"].get<float>();
	LOG(std::string(std::string("GOAL SCORED AT TIMESTAMP: ") + std::string(gameInfo["matchInfo"]["secondsElapsed"].get<float>())));

	if (!caller.IsNull()) {
		LOG("Caller is NOT null");
		auto playerPRI = caller.GetPRI();
		if (!playerPRI.IsNull()) {
			LOG("playerPRI is NOT null");
			auto playerName = playerPRI.GetPlayerName();
			if (!playerName.IsNull()) {
				LOG("playerName is NOT null");
				lastGoal["playerName"] = playerName.ToString();
			}
			lastGoal["playerSteamID"] = playerPRI.GetUniqueId().ID;
			auto playersTeam = playerPRI.GetTeam();
			if (!playersTeam.IsNull()) {
				int teamIdx = playersTeam.GetTeamIndex();
				
			}
		}
	}
}

void RocketLeagueGSIPlugin::update() {
	using namespace std::chrono_literals;
	while (running) {
		gameWrapper->Execute([&](GameWrapper * gw) {
			//HERE GET ALL THE VALUES FROM THE GAME
			
			if (gw->IsInOnlineGame()) {
				if (!gw->IsSpectatingInOnlineGame() && !gw->IsInReplay()) {
					if (!gw->GetOnlineGame().IsNull()) {
						nlohmann::json matchInfo;
						matchInfo["inGame"] = true;
						matchInfo["inOnlineGame"] = true;

						ServerWrapper match = gw->GetOnlineGame();

						if (!match.GetbMatchEnded()) {
							if (match.GetbOverTime()) { //Check if in overtime
								matchInfo["inOvertime"] = true;
							}
							matchInfo["matchEnded"] = false;
							matchInfo["secondsRemaining"] = match.GetSecondsRemaining();
							matchInfo["secondsElapsed"] = match.GetSecondsElapsed();
							matchInfo["matchLength"] = match.GetGameTime();

							ArrayWrapper<TeamWrapper> teams = match.GetTeams();
							ArrayWrapper<PriWrapper> players = match.GetPRIs(); //All players on a server
							nlohmann::json teamsJSON = nlohmann::json::array();

							for (int i = 0; i < teams.Count(); i++) { //Getting Teams info
								auto currentTeam = teams.Get(i);
								if (!currentTeam.IsNull()) {
									nlohmann::json currentTeamJSON;
									int currentTeamIdx = currentTeam.GetTeamIndex();
									if (!currentTeam.GetCustomTeamName().IsNull()) {
										currentTeamJSON["name"] = currentTeam.GetCustomTeamName().ToString();
									}
									else {
										if (!currentTeam.GetTeamName().IsNull()) {
											currentTeamJSON["name"] = currentTeam.GetTeamName().ToString();
										}
										else {
											currentTeamJSON["name"] = std::string( std::string("Team") + std::to_string(currentTeamIdx) );
										}
									}

									//Team score
									currentTeamJSON["score"] = currentTeam.GetScore();
									//Team index
									currentTeamJSON["index"] = currentTeamIdx;

									//Team Color
									nlohmann::json teamCol;
									teamCol["r"] = (int)(currentTeam.GetFontColor().R * 255);
									teamCol["g"] = (int)(currentTeam.GetFontColor().G * 255);
									teamCol["b"] = (int)(currentTeam.GetFontColor().B * 255);
									teamCol["a"] = (int)(currentTeam.GetFontColor().A * 255);
									
									currentTeamJSON["color"] = teamCol;

									//ArrayWrapper<PriWrapper> teamPlayers = currentTeam.GetMembers(); //Doesn't work in online games (array size always 0)

									nlohmann::json playersJSON = nlohmann::json::array();

									for (int j = 0; j < players.Count(); j++) { //Iterating through all players on the server
										auto currentPlayer = players.Get(j);
										if (!currentPlayer.IsNull()) {
											nlohmann::json currentPlayerJSON;
											auto playersTeam = currentPlayer.GetTeam();
											if (!playersTeam.IsNull()) {
												if (playersTeam.GetTeamIndex() == currentTeamIdx) { //Player is in currently checked team
													auto playerName = currentPlayer.GetPlayerName();
													if (!playerName.IsNull()) {
														currentPlayerJSON["name"] = playerName.ToString();
													}
													currentPlayerJSON["spectator"] = true;
													auto playerIP = currentPlayer.GetPublicIP();
													if (!playerIP.IsNull()) {
														currentPlayerJSON["ip"] = playerIP.ToString();
													}
													int playerID = currentPlayer.GetPlayerID();
													currentPlayerJSON["playerID"] = playerID;
													currentPlayerJSON["steamID"] = currentPlayer.GetUniqueId().ID;
													if (!currentPlayer.GetbIsSpectator()) { //Player is not a spectator
														currentPlayerJSON["spectator"] = false;
														currentPlayerJSON["score"] = currentPlayer.GetMatchScore();
														currentPlayerJSON["goals"] = currentPlayer.GetMatchGoals();
														currentPlayerJSON["assists"] = currentPlayer.GetMatchAssists();
														currentPlayerJSON["saves"] = currentPlayer.GetMatchSaves();
														currentPlayerJSON["shots"] = currentPlayer.GetMatchShots();
													}
													playersJSON.push_back(currentPlayerJSON);
												}
											}
										}
									}
									currentTeamJSON["players"] = playersJSON;
									teamsJSON.push_back(currentTeamJSON);
								}
							}
							matchInfo["teams"] = teamsJSON;

							if (!match.GetGameBalls().Get(0).IsNull()) { //Getting Ball info
								BallWrapper ball = match.GetGameBalls().Get(0);
							}

							if (!gw->GetLocalCar().IsNull()) { //Getting Loacl Player info

								CarWrapper localCar = gw->GetLocalCar();
								nlohmann::json player;
								player["x"] = localCar.GetLocation().X;
								player["y"] = localCar.GetLocation().Y;
								player["z"] = localCar.GetLocation().Z;
								player["velocity"] = localCar.GetVelocity().magnitude();
								if (!localCar.GetBoostComponent().IsNull()) {
									player["currentBoostAmount"] = localCar.GetBoostComponent().GetCurrentBoostAmount();
								}
								gameInfo["localPlayer"] = player;
							}
							matchInfo["lastGoal"] = lastGoal;
							gameInfo["matchInfo"] = matchInfo;
						} // !MatchEnded
						else {
							gameInfo["matchInfo"]["matchEnded"] = true;
							gameInfo["matchInfo"]["lastGoal"]["noGoalsYet"] = true;
						}
					} // GetOnlineGame is null
				}
				else if (gw->IsSpectatingInOnlineGame()) {
					gameInfo["matchInfo"]["inGame"] = false;
				}
				else if (gw->IsInReplay()) {
					gameInfo["matchInfo"]["lastGoal"]["noGoalsYet"] = true;
					gameInfo["matchInfo"]["inGame"] = false;
				}
			}
			else if(gw->IsInGame()) {
				if (!gw->IsInReplay()) {
					nlohmann::json matchInfo;
					matchInfo["inGame"] = true;
					matchInfo["inOnlineGame"] = true;

					ServerWrapper match = gw->GetGameEventAsServer();

					if (!match.GetbMatchEnded()) {
						if (match.GetbOverTime()) { //Check if in overtime
							matchInfo["inOvertime"] = true;
						}
						matchInfo["matchEnded"] = false;
						matchInfo["secondsRemaining"] = match.GetSecondsRemaining();
						matchInfo["secondsElapsed"] = match.GetSecondsElapsed();
						matchInfo["matchLength"] = match.GetGameTime();

						ArrayWrapper<TeamWrapper> teams = match.GetTeams();
						ArrayWrapper<PriWrapper> players = match.GetPRIs(); //All players on a server
						nlohmann::json teamsJSON = nlohmann::json::array();

						for (int i = 0; i < teams.Count(); i++) { //Getting Teams info
							auto currentTeam = teams.Get(i);
							if (!currentTeam.IsNull()) {
								nlohmann::json currentTeamJSON;
								int currentTeamIdx = currentTeam.GetTeamIndex();
								if (!currentTeam.GetCustomTeamName().IsNull()) {
									currentTeamJSON["name"] = currentTeam.GetCustomTeamName().ToString();
								}
								else {
									if (!currentTeam.GetTeamName().IsNull()) {
										currentTeamJSON["name"] = currentTeam.GetTeamName().ToString();
									}
									else {
										currentTeamJSON["name"] = std::string(std::string("Team") + std::to_string(currentTeamIdx));
									}
								}

								//Team score
								currentTeamJSON["score"] = currentTeam.GetScore();
								//Team index
								currentTeamJSON["index"] = currentTeamIdx;

								//Team Color
								nlohmann::json teamCol;
								teamCol["r"] = (int)(currentTeam.GetFontColor().R * 255);
								teamCol["g"] = (int)(currentTeam.GetFontColor().G * 255);
								teamCol["b"] = (int)(currentTeam.GetFontColor().B * 255);
								teamCol["a"] = (int)(currentTeam.GetFontColor().A * 255);

								currentTeamJSON["color"] = teamCol;

								//ArrayWrapper<PriWrapper> teamPlayers = currentTeam.GetMembers(); //Doesn't work in online games (array size always 0)

								nlohmann::json playersJSON = nlohmann::json::array();

								for (int j = 0; j < players.Count(); j++) { //Iterating through all players on the server
									auto currentPlayer = players.Get(j);
									if (!currentPlayer.IsNull()) {
										nlohmann::json currentPlayerJSON;
										auto playersTeam = currentPlayer.GetTeam();
										if (!playersTeam.IsNull()) {
											if (playersTeam.GetTeamIndex() == currentTeamIdx) { //Player is in currently checked team
												auto playerName = currentPlayer.GetPlayerName();
												if (!playerName.IsNull()) {
													currentPlayerJSON["name"] = playerName.ToString();
												}
												currentPlayerJSON["spectator"] = true;
												auto playerIP = currentPlayer.GetPublicIP();
												if (!playerIP.IsNull()) {
													currentPlayerJSON["ip"] = playerIP.ToString();
												}
												int playerID = currentPlayer.GetPlayerID();
												currentPlayerJSON["playerID"] = playerID;
												currentPlayerJSON["steamID"] = currentPlayer.GetUniqueId().ID;
												if (!currentPlayer.GetbIsSpectator()) { //Player is not a spectator
													currentPlayerJSON["spectator"] = false;
													currentPlayerJSON["score"] = currentPlayer.GetMatchScore();
													currentPlayerJSON["goals"] = currentPlayer.GetMatchGoals();
													currentPlayerJSON["assists"] = currentPlayer.GetMatchAssists();
													currentPlayerJSON["saves"] = currentPlayer.GetMatchSaves();
													currentPlayerJSON["shots"] = currentPlayer.GetMatchShots();
												}
												playersJSON.push_back(currentPlayerJSON);
											}
										}
									}
								}
								currentTeamJSON["players"] = playersJSON;
								teamsJSON.push_back(currentTeamJSON);
							}
						}
						matchInfo["teams"] = teamsJSON;

						if (!match.GetGameBalls().Get(0).IsNull()) { //Getting Ball info
							BallWrapper ball = match.GetGameBalls().Get(0);
						}

						if (!gw->GetLocalCar().IsNull()) { //Getting Loacl Player info

							CarWrapper localCar = gw->GetLocalCar();
							nlohmann::json player;
							player["x"] = localCar.GetLocation().X;
							player["y"] = localCar.GetLocation().Y;
							player["z"] = localCar.GetLocation().Z;
							player["velocity"] = localCar.GetVelocity().magnitude();
							if (!localCar.GetBoostComponent().IsNull()) {
								player["currentBoostAmount"] = localCar.GetBoostComponent().GetCurrentBoostAmount();
							}
							gameInfo["localPlayer"] = player;
						}
						matchInfo["lastGoal"] = lastGoal;
						gameInfo["matchInfo"] = matchInfo;
					} // !MatchEnded
				}
				else if (gw->IsInReplay()) {
					gameInfo["matchInfo"]["lastGoal"]["noGoalsYet"] = true;
					gameInfo["matchInfo"]["inGame"] = false;
				}
			}
			else {
				gameInfo["matchInfo"]["lastGoal"]["noGoalsYet"] = true;
				gameInfo["matchInfo"]["inGame"] = false;
			}
		});
		std::this_thread::sleep_for(50ms); //UPDATE RATE
	}
}

bool RocketLeagueGSIPlugin::isRunning() {
	return running;
}

void RocketLeagueGSIPlugin::testGSI(std::vector<std::string> events) {
	LOG(std::string("GetbOverTime: ") + std::to_string(gameWrapper->GetGameEventAsServer().GetbOverTime()));
	LOG(std::string("GetSecondsRemaining: ") + std::to_string(gameWrapper->GetGameEventAsServer().GetSecondsRemaining()));
	LOG(std::string("GetGameTime: ") + std::to_string(gameWrapper->GetGameEventAsServer().GetGameTime()));
}

std::string RocketLeagueGSIPlugin::gameInfoToString() {
	return gameInfo.dump();
}


