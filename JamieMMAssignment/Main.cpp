#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

static unsigned int SERVER_PORT = 65000;
static unsigned int CLIENT_PORT = 65001;
static unsigned int MAX_CONNECTIONS = 4;

enum NetworkState
{
	NS_Init = 0,
	NS_PendingStart,
	NS_Started,
	NS_Lobby,
	NS_CharacterSelect,
	NS_ReadyUp,
	NS_Pending,
	NS_InGame,
	NS_PlayerTurn,
};

bool isServer = false;
bool isRunning = true;
int g_currentTurn = 0;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;

enum {
	ID_THEGAME_LOBBY_READY = ID_USER_PACKET_ENUM,
	ID_PLAYER_READY,
	ID_INVAILDINFO,
	ID_READYUP,
	ID_CANREADYUP,
	ID_PLAYERREADY_TOPLAY,
	ID_CHARACTERSELECT,
	ID_CANSTART_GAME,
	ID_THEGAME_START,
	ID_PLAYERTARGET,
	ID_WHOS_TURN,
	ID_ATTACK,
	ID_WHOTO_ATTACK,
	ID_PLAYERSTATS,
	ID_WINNER,
};

enum EPlayerClass
{
	Mage = 0,
	Rogue,
	Cleric,
};

struct SPlayer
{
	std::string m_name;
	unsigned int m_health;
	unsigned int m_defence;
	unsigned int m_turn;
	EPlayerClass m_class;
	bool m_isReady = false;
	RakNet::SystemAddress m_address;


	//function to send a packet with name/health/class etc
	void SendName(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	void SendReady(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_READYUP);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	void SendStats(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYERSTATS);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);
		RakNet::RakString health(std::to_string(m_health).c_str());
		writeBs.Write(health);
		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	void SendNameAsTarget(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_WHOTO_ATTACK);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);
		
		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	void SendNameAttacked(RakNet::SystemAddress systemAddress, RakNet::RakString attacker, int damage, bool isBroadcast)
	{
		RakNet::BitStream bsWrite;
		bsWrite.Write((RakNet::MessageID)ID_PLAYERTARGET);
		RakNet::RakString action("Attack");
		bsWrite.Write(action);
		RakNet::RakString playerName(attacker);
		bsWrite.Write(playerName);
		RakNet::RakString targetName(m_name.c_str());
		bsWrite.Write(targetName);

		if (damage < 0)
		{
			RakNet::RakString damageDone(std::to_string(0).c_str());
			bsWrite.Write(damageDone);
		}
		else
		{
			RakNet::RakString damageDone(std::to_string(damage).c_str());
			bsWrite.Write(damageDone);
		}

		printf("struct\n");
		printf("%s - attacked player \n", m_name.c_str());
		printf("%s - player attacking \n", attacker.C_String());
		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}
};

std::map<unsigned long, SPlayer> m_players;

SPlayer& GetPlayer(RakNet::RakNetGUID raknetId)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(raknetId);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
	assert(it != m_players.end());
	return it->second;
}

void OnLostConnection(RakNet::Packet* packet)
{
	SPlayer& lostPlayer = GetPlayer(packet->guid);
	lostPlayer.SendName(RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	unsigned long keyVal = RakNet::RakNetGUID::ToUint32(packet->guid);
	m_players.erase(keyVal);
}

//server
void OnIncomingConnection(RakNet::Packet* packet)
{
	//must be server in order to recieve connection
	assert(isServer);
	m_players.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), SPlayer()));
	std::cout << "Total Players: " << m_players.size() << std::endl;
}

//client
void OnConnectionAccepted(RakNet::Packet* packet)
{
	//server should not ne connecting to anybody, 
	//clients connect to server
	assert(!isServer);
	g_networkState_mutex.lock();
	g_networkState = NS_Lobby;
	g_networkState_mutex.unlock();
	g_serverAddress = packet->systemAddress;
}

//this is on the client side
void DisplayPlayerReady(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " has joined" << std::endl;
}

//this is on the client side
void DisplayPlayerReadyPlay(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " has Ready up." << std::endl;
}

void OnLobbyReady(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	SPlayer& player = GetPlayer(packet->guid);
	player.m_name = userName;
	std::cout << userName.C_String() << " aka " << player.m_name.c_str() << " IS READY!!!!!" << std::endl;

	//notify all other connected players that this plyer has joined the game
	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		//skip over the player who just joined
		if (guid == it->first)
		{
			continue;
		}

		SPlayer& player = it->second;
		player.SendName(packet->systemAddress, false);
		/*RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(player.m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));*/
	}

	player.SendName(packet->systemAddress, true);
	/*RakNet::BitStream writeBs;
	writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
	RakNet::RakString name(player.m_name.c_str());
	writeBs.Write(name);
	assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));*/

}

void InvaildInfo(RakNet::Packet* packet)
{
	RakNet::BitStream readBs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	readBs.Read(messageId);
	RakNet::RakString reason;
	readBs.Read(reason);

	if (reason == "class")
	{
		printf("Invaild. Try Again. \n");
		g_networkState_mutex.lock();
		g_networkState = NS_CharacterSelect;
		g_networkState_mutex.unlock();
	}
}

//Sets player class info server
void PlayerCharacterSelect(RakNet::Packet* packet)
{
	assert(isServer);
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString character;
	bs.Read(character);

	SPlayer& player = GetPlayer(packet->guid);

	if (character == "mage" || character == "Mage")
	{
		printf("%s is a mage\n",player.m_name.c_str() );
		player.m_class = Mage;
		player.m_defence = rand() % 9 + 5;
		player.m_health = 100;
		player.m_turn = 0;
		player.m_address = packet->systemAddress;

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_CANREADYUP);
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
	else if (character == "rogue" || character == "Rogue")
	{
		printf("%s is a Rogue\n", player.m_name.c_str());
		player.m_class = Rogue;
		player.m_defence = rand() % 9 + 5;
		player.m_health = 100;
		player.m_turn = 0;
		player.m_address = packet->systemAddress;

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_CANREADYUP);
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
	else if (character == "cleric" || character == "Cleric")
	{
		printf("%s is a Cleric\n", player.m_name.c_str());
		player.m_class = Cleric;
		player.m_defence = rand() % 10 + 8;
		player.m_health = 100;
		player.m_turn = 0;
		player.m_address = packet->systemAddress;

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_CANREADYUP);
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
	else
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_INVAILDINFO);
		RakNet::RakString reason("class");
		writeBs.Write(reason);

		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
}

void PlayerReadyToStartGame(RakNet::Packet* packet)
{
	RakNet::BitStream readBs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	readBs.Read(messageId);
	RakNet::RakString Ready;
	readBs.Read(Ready);

	int peopleReady = 0;

	if (Ready == "yes" || Ready == "Yes")
	{
		unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
		SPlayer& player = GetPlayer(packet->guid);
		player.m_isReady = true;
		printf("%s is Ready. \n", player.m_name.c_str());

		for (auto it : m_players)
		{
			if (guid == it.first)
			{
				continue;
			}
			SPlayer& player = it.second;
			if (player.m_isReady)
				player.SendReady(packet->systemAddress, false);
		}
		player.SendReady(packet->systemAddress, true);
	}
	else
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_CANREADYUP);
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}

	

	for (auto it : m_players)
	{

		SPlayer& player = it.second;
		if (player.m_isReady)
			peopleReady++;
	}
	if (peopleReady >= 3)
	{
		g_networkState_mutex.lock();
		g_networkState = NS_InGame;
		g_networkState_mutex.unlock();

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_CANSTART_GAME);
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
}

void StartGame(RakNet::Packet* packet)
{

	std::cout << "Game is Starting now" << std::endl;

	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_THEGAME_START);
	RakNet::RakString playerturn("Turn");
	bs.Write(playerturn);

	assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));

}

//this is on the client side
void DisplayActivePlayer(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);
	if (userName == "Mine")
	{
		std::cout << "It is your Turn." << std::endl;
		std::cout << "Who do you want to attack?" << std::endl;

		g_networkState_mutex.lock();
		g_networkState = NS_PlayerTurn;
		g_networkState_mutex.unlock();

		RakNet::BitStream bsWrite;
		bsWrite.Write((RakNet::MessageID)ID_THEGAME_START);
		RakNet::RakString possibleTargets("toAttack");
		bsWrite.Write(possibleTargets);

		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
	}
	else {
		std::cout << "It's " << userName.C_String() << " turn." << std::endl;
		g_networkState_mutex.lock();
		g_networkState = NS_InGame;
		g_networkState_mutex.unlock();
	}
}

//Display all the players alive that the active player can attack
void Targets(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString attackTarget;
		bs.Read(attackTarget);
		printf("you can attack %s.\n", attackTarget.C_String());
}

//Display who was attack and the damage done that that player.
void PlayerHasAttack(RakNet::Packet* packet)
{
	RakNet::BitStream bsRead(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bsRead.Read(messageId);
	RakNet::RakString action;
	bsRead.Read(action);
	RakNet::RakString targetPlayer;
	bsRead.Read(targetPlayer);
	RakNet::RakString attackingPlayer;
	bsRead.Read(attackingPlayer);
	RakNet::RakString damageToPlayer;
	bsRead.Read(damageToPlayer);

	if (action == "Attack")
	{
		printf("%s was attacked by %s and lost %s health.\n", attackingPlayer.C_String(), targetPlayer.C_String(), damageToPlayer.C_String());
	
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_THEGAME_START);
		RakNet::RakString playerturn("Turn");
		bs.Write(playerturn);

		g_networkState_mutex.lock();
		g_networkState = NS_Pending;
		g_networkState_mutex.unlock();

		assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
	}
	else if (action == "invaild")
	{
		printf("inVaild Target. Try Again. \n");
		g_networkState_mutex.lock();
		g_networkState = NS_PlayerTurn;
		g_networkState_mutex.unlock();
	}

}

void AttackedTarget(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString Target;
	bs.Read(Target);

	SPlayer& attacker = GetPlayer(packet->guid);
	bool vaild = false;
	int damage = 0;

	printf("function\n");
	//makes sure the target is a vaild target 
	printf("%s - attacked player. \n", Target.C_String());
	for (auto it : m_players)
	{
		SPlayer& targetPlayer = it.second;

		if (Target == targetPlayer.m_name.c_str())
		{
			attacker.m_turn++;
			damage = (rand() % 15 + 10) - targetPlayer.m_defence;
			if (damage <= 0)
			{
				damage = 0;
			
			}
			unsigned int health = targetPlayer.m_health -= damage;
			targetPlayer.m_health = health;

			printf("%s %s Health \n", targetPlayer.m_name.c_str(), std::to_string(targetPlayer.m_health).c_str());
			targetPlayer.SendNameAttacked(packet->systemAddress, attacker.m_name.c_str(), damage, true);
			targetPlayer.SendNameAttacked(packet->systemAddress, attacker.m_name.c_str(), damage, false);
			vaild = true;
			break;
		}

	}

	if (!vaild)
	{
		printf("Invaild");

		RakNet::BitStream bsWrite;
		bsWrite.Write((RakNet::MessageID)ID_PLAYERTARGET);
		RakNet::RakString action("invaild");
		bsWrite.Write(action);

		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));

	}
}

//Displays players name and health
void DisplayStats(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString playerAlive;
	bs.Read(playerAlive);
	RakNet::RakString playerHealth;
	bs.Read(playerHealth);


	printf("%s has %s health left.\n", playerAlive.C_String(), playerHealth.C_String());
}

void GameManager(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString whatToDo;
	bs.Read(whatToDo);

	printf("%s.\n", GetPlayer(packet->guid).m_name.c_str());
	int peopleAlive = 0;

	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{

		SPlayer& player = it->second;
		if (player.m_health >= 0)
			peopleAlive++;

	}


	RakNet::BitStream bsWrite;
	if (whatToDo == "Turn")
	{
		bool findingPlayer = false;
		while (!findingPlayer)
		{
			for (auto it : m_players)
			{

				SPlayer& player = it.second;

				if (player.m_turn <= g_currentTurn)
				{
					SPlayer callingPlayer = GetPlayer(packet->guid);
					if (player.m_address == packet->systemAddress)
					{
						bsWrite.Write((RakNet::MessageID)ID_WHOS_TURN);
						RakNet::RakString name("Mine");
						bsWrite.Write(name);
						assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
					}
					else
					{
						bsWrite.Write((RakNet::MessageID)ID_WHOS_TURN);
						RakNet::RakString name(player.m_name.c_str());
						bsWrite.Write(name);
						assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
					}
					findingPlayer = true;
					break;
					
				}
			}
			if (!findingPlayer)
				g_currentTurn++;
		}

	}
	else if (whatToDo == "stats")
	{

		printf(" %s asked for stats.\n", GetPlayer(packet->guid).m_name.c_str());
		for (auto it : m_players)
		{


			SPlayer& player = it.second;
			printf("%s given stats. \n", player.m_name.c_str());
			if (player.m_health > 0)
			{
				player.SendStats(packet->systemAddress, false);
				
			}

		}

	}
	else if (whatToDo == "toAttack")
	{
		unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
		for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		{
			if (guid == it->first)
			{
				continue;
			}

			SPlayer& player = it->second;
			if (player.m_health > 0)
			{
				printf("%s. \n ", player.m_name.c_str());
				player.SendNameAsTarget(packet->systemAddress, false);

			}
		}

	}

	//End Condition
	if (peopleAlive <= 1)
	{
		for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		{
			SPlayer& winningPlayer = it->second;
			if (winningPlayer.m_health < 0)
			{
				bsWrite.Write((RakNet::MessageID)ID_WINNER);
				RakNet::RakString winner(winningPlayer.m_name.c_str());
				bsWrite.Write(winner);
				assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));
			}
		}
	}
}


unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}


void InputHandler()
{
	assert(!isServer);
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Init)
		{
			std::cout << "press (s) for server (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = (userInput[0] == 's');
			g_networkState_mutex.lock();
			g_networkState = NS_PendingStart;
			g_networkState_mutex.unlock();

		}
		else if (g_networkState == NS_Lobby)
		{
			std::cout << "Enter your name to play or type quit to leave" << std::endl;
			std::cin >> userInput;
			//quitting is not acceptable in our game, create a crash to teach lesson
			assert(strcmp(userInput, "quit"));

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY);
			RakNet::RakString name(userInput);
			bs.Write(name);

			//returns 0 when something is wrong
			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_CharacterSelect;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_CharacterSelect)
		{
			std::cout << "Pick a your class. Mage, Rogue, Cleric" << std::endl;
			std::cin >> userInput;

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_CHARACTERSELECT);
			RakNet::RakString character(userInput);
			bs.Write(character);

			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_ReadyUp)
		{
			std::cout << "Are you ready? yes or no" << std::endl;

			std::cin >> userInput;

			RakNet::BitStream writeBs;
			writeBs.Write((RakNet::MessageID)ID_PLAYERREADY_TOPLAY);
			RakNet::RakString answer(userInput);
			writeBs.Write(answer);

			assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));

			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_InGame)
		{
			std::cout << "you can type stats to check the stats of player alive." << std::endl;
			std::cin >> userInput;

			RakNet::BitStream bsWrite;
			bsWrite.Write((RakNet::MessageID)ID_THEGAME_START);
			RakNet::RakString Target(userInput);
			bsWrite.Write(Target);

			assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
		}
		else if (g_networkState == NS_PlayerTurn)
		{
			std::cin >> userInput;

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_ATTACK);
			RakNet::RakString target(userInput);
			bs.Write(target);

			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));

			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_Pending)
		{
			//static bool doOnce = false;
			//if (!doOnce)
			//	std::cout << "pending..." << std::endl;

			//doOnce = true;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

bool HandleLowLevelPackets(RakNet::Packet* packet)
{
	bool isHandled = true;
	// We got a packet, get the identifier with our handy function
	unsigned char packetIdentifier = GetPacketIdentifier(packet);

	// Check if this is a network message packet
	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
		//client connecting to server
		OnIncomingConnection(packet);
		printf("ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		OnIncomingConnection(packet);
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		// Sorry, the server is full.  I don't do anything here but
		// A real app should tell the user
		printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
		break;

	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;

	case ID_CONNECTION_LOST:
		// Couldn't deliver a reliable packet - i.e. the other system was abnormally
		// terminated
		printf("ID_CONNECTION_LOST\n");
		OnLostConnection(packet);
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		//printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		//printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
		OnConnectionAccepted(packet);
		break;
	case ID_CONNECTED_PING:
	case ID_UNCONNECTED_PING:
		printf("Ping from %s\n", packet->systemAddress.ToString(true));
		break;
	default:
		isHandled = false;
		break;
	}
	return isHandled;
}

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			if (!HandleLowLevelPackets(packet))
			{
				//our game specific packets
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case ID_THEGAME_LOBBY_READY:
					OnLobbyReady(packet);
					break;
				case ID_CHARACTERSELECT:
					PlayerCharacterSelect(packet);
					break;
				case ID_INVAILDINFO:
					InvaildInfo(packet);
					break;
				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;
				case ID_PLAYERREADY_TOPLAY:
					PlayerReadyToStartGame(packet);
					break;
				case ID_READYUP:
					DisplayPlayerReadyPlay(packet);
					break;
				case ID_CANREADYUP:
					g_networkState_mutex.lock();
					g_networkState = NS_ReadyUp;
					g_networkState_mutex.unlock();
					break;
				case ID_CANSTART_GAME:
					StartGame(packet);
					break;
				case ID_THEGAME_START:
					GameManager(packet);
					break;
				case ID_WHOS_TURN:
					DisplayActivePlayer(packet);
					break;
				case ID_WHOTO_ATTACK:
					Targets(packet);
					break;
				case ID_PLAYERSTATS:
					DisplayStats(packet);
					break;
				case ID_ATTACK:
					AttackedTarget(packet);
					break;
				case ID_PLAYERTARGET:
					PlayerHasAttack(packet);
					break;
				default:
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);

	while (isRunning)
	{
		if (g_networkState == NS_PendingStart)
		{
			if (isServer)
			{
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4

				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				//ensures we are server
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				std::cout << "server started" << std::endl;
				g_networkState_mutex.lock();
				g_networkState = NS_Started;
				g_networkState_mutex.unlock();
			}
			//client
			else
			{
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, 0);
				socketDescriptor.socketFamily = AF_INET;

				while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
					socketDescriptor.port++;

				RakNet::StartupResult result = g_rakPeerInterface->Startup(8, &socketDescriptor, 1);
				assert(result == RakNet::RAKNET_STARTED);

				g_networkState_mutex.lock();
				g_networkState = NS_Started;
				g_networkState_mutex.unlock();

				g_rakPeerInterface->SetOccasionalPing(true);
				//"127.0.0.1" = local host = your machines address
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("192.168.0.10", SERVER_PORT, nullptr, 0);
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "client attempted connection..." << std::endl;

			}
		}

	}

	//std::cout << "press q and then return to exit" << std::endl;
	//std::cin >> userInput;

	inputHandler.join();
	packetHandler.join();
	return 0;
}