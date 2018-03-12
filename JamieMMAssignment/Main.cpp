#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>

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
	NS_InGame,
	NS_PlayerTurn,
	NS_GameOver,
	NS_Pending,
};

bool isServer = false;
bool isRunning = true;
int g_currentTurn = 0;
int g_player = 0;


RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;

enum {
	ID_THEGAME_LOBBY_READY = ID_USER_PACKET_ENUM,
	ID_PLAYER_READY,
	ID_PLAYER_CHARACTER,
	ID_PLAYER_CLASSPICKED,
	ID_START_GAME,
	ID_WHOS_TURN,
	ID_PLAYERTARGET,
	ID_PLAYERSTATS,
	ID_WHOTOATTACK,
	ID_ATTACK,
	ID_WINNER,
	ID_THEGAME_START,
};

enum EPlayerClass
{
	Mage = 0,
	Rogue,
	Cleric,
	Homeless,
};

struct SPlayer
{
	std::string m_name;
	unsigned int m_health;
	unsigned int m_turn;
	bool m_activePlayer;
	EPlayerClass m_class;
	bool m_isReady = false;
	bool m_alive = true;
	RakNet::SystemAddress address;

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
	//function to send a packet with name/health/class etc
	void SendClass(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_CLASSPICKED);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}
	void SendStats(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYERSTATS);
		RakNet::RakString playerName(m_name.c_str());
		writeBs.Write(playerName);
		RakNet::RakString playerHealth(std::to_string(m_health).c_str());
		writeBs.Write(playerHealth);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}
};

std::map<RakNet::SystemAddress, SPlayer> m_players;

SPlayer& GetPlayer(RakNet::SystemAddress address)
{
	RakNet::SystemAddress guid = address;
	std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.find(guid);
	assert(it != m_players.end());
	return it->second;
}

void OnLostConnection(RakNet::Packet* packet)
{
	SPlayer& lostPlayer = GetPlayer(packet->systemAddress);
	lostPlayer.SendName(RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	RakNet::SystemAddress keyVal = packet->systemAddress;
	m_players.erase(keyVal);
}

//server
void OnIncomingConnection(RakNet::Packet* packet)
{
	//must be server in order to recieve connection
	assert(isServer);
	if (g_networkState != NS_InGame) {
		m_players.insert(std::make_pair(packet->systemAddress, SPlayer()));
		std::cout << "Total Players: " << m_players.size() << std::endl;
	}
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
void DisplayPickedClass(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " has picked their class." << std::endl;
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

void DisplayWinner(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " wins." << std::endl;
	std::cout << "Game Over" << std::endl;

	g_networkState_mutex.lock();
	g_networkState = NS_GameOver;
	g_networkState_mutex.unlock();
}

void OnLobbyReady(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	RakNet::SystemAddress guid = packet->systemAddress;
	SPlayer& player = GetPlayer(packet->systemAddress);
	player.m_name = userName;
	std::cout << userName.C_String() << " aka " << player.m_name.c_str() << " IS READY!!!!!" << std::endl;

	//notify all other connected players that this plyer has joined the game
	for (std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		//skip over the player who just joined
		if (guid == it->first)
		{
			continue;
		}

		SPlayer& player = it->second;
		player.SendName(packet->systemAddress, false);

	}

	player.SendName(packet->systemAddress, true);


}

//Sets player class info server
void PlayerCharacterSelect(RakNet::Packet* packet)
{

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString character;
	bs.Read(character);

	SPlayer& player = GetPlayer(packet->systemAddress);

	if (character == "Mage" || character == "mage")
	{
		player.m_class = Mage;
		player.m_isReady = true;
		player.m_health = 100;
		player.m_turn = 0;
		player.m_activePlayer = false;
		player.address = packet->systemAddress;
	}
	else if (character == "Rogue" || character == "rogue")
	{
		player.m_class = Rogue;
		player.m_isReady = true;
		player.m_health = 100;
		player.m_turn = 0;
		player.m_activePlayer = false;
		player.address = packet->systemAddress;
	}
	else if (character == "Cleric" || character == "cleric")
	{
		player.m_class = Cleric;
		player.m_isReady = true;
		player.m_health = 100;
		player.m_turn = 0;
		player.m_activePlayer = false;
		player.address = packet->systemAddress;
	}
	else
	{
		player.m_class = Homeless;
		player.m_isReady = true;
		player.m_health = 50;
		player.m_turn = 0;
		player.m_activePlayer = false;
		player.address = packet->systemAddress;
	}

	RakNet::SystemAddress guid = packet->systemAddress;
	//notify all other connected players that this plyer has joined the game
	for (std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		//skip over the player who just joined
		if (guid == it->first)
		{
			continue;
		}

		SPlayer& player = it->second;
		player.SendClass(packet->systemAddress, false);

	}

	player.SendClass(packet->systemAddress, true);

	int peopleReady = 0;

	for (std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{

		player = it->second;
		if (player.m_isReady)
			peopleReady++;

	}

	if (m_players.size() >= 2 && peopleReady == m_players.size())
	{

		RakNet::BitStream bsWrite;
		bsWrite.Write((RakNet::MessageID)ID_START_GAME);
		RakNet::RakString StartGame("Game is starting now.");
		bsWrite.Write(StartGame);

		//tells all client to print game is starting
		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));
		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));

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

//Displays players name and health
void GetPlayerAlive(RakNet::Packet* packet)
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
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString action;
	bs.Read(action);
	RakNet::RakString targetPlayer;
	bs.Read(targetPlayer);
	RakNet::RakString attackingPlayer;
	bs.Read(attackingPlayer);
	RakNet::RakString damageToPlayer;
	bs.Read(damageToPlayer);
	
	if (action == "Attack")
		printf("%s was attacked by %s and lost %s health.\n", targetPlayer.C_String(), attackingPlayer.C_String(), damageToPlayer.C_String());
	else if (action == "invaild")
		printf("inVaild Target. Try Again. \n");

}

void AttackedTarget(RakNet::Packet* packet)
{
	RakNet::BitStream bs;
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString Target;
	bs.Read(Target);

	SPlayer& attacker = GetPlayer(packet->systemAddress);
	SPlayer& targetPlayer = GetPlayer(packet->systemAddress);
	bool vaild = false;

	//makes sure the target is a vaild target 
	printf("%s. \n", Target);
	for (auto it: m_players)
	{
		 targetPlayer = it.second;
		
		if (Target == targetPlayer.m_name.c_str())
		{
			vaild = true;
			printf("Vaild target.");
			break;
		}
		
	}

	if (vaild)
	{
		attacker.m_turn++;

		targetPlayer.m_health -= 10;
		RakNet::BitStream bsWrite;
		bsWrite.Write((RakNet::MessageID)ID_PLAYERTARGET);
		RakNet::RakString action("Attack");
		bsWrite.Write(action);
		RakNet::RakString targetName(targetPlayer.m_name.c_str());
		bsWrite.Write(targetName);
		RakNet::RakString playerName(attacker.m_name.c_str());
		bsWrite.Write(playerName);
		RakNet::RakString damageDone(std::to_string(10).c_str());
		bsWrite.Write(damageDone);
		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));
	}
	else
	{
		printf("Invaild");

		targetPlayer.m_health -= 10;
		RakNet::BitStream bsWrite;
		bsWrite.Write((RakNet::MessageID)ID_PLAYERTARGET);
		RakNet::RakString action("invaild");
		bsWrite.Write(action);
		RakNet::RakString targetName(targetPlayer.m_name.c_str());
		bsWrite.Write(targetName);

		assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
}

void GameManager(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString whatToDo;
	bs.Read(whatToDo);

	printf("%s.\n", GetPlayer(packet->systemAddress).m_name.c_str());
	int peopleAlive = 0;

	for (std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{

		SPlayer& player = it->second;
		if (player.m_health >= 0)
			peopleAlive++;

	}


	RakNet::BitStream bsWrite;
	if (whatToDo == "Turn")
	{
		
		for (auto it: m_players)
		{
		
			SPlayer& player = it.second;
		/*	if (player.m_activePlayer)
			{
				if (player.address == packet->systemAddress)
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
				break;
			}*/

			if (player.m_turn <= g_currentTurn)
			{
				SPlayer callingPlayer = GetPlayer(packet->systemAddress);
				if (player.address == packet->systemAddress)
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
				player.m_activePlayer = true;
				break;
			}
		}


	}
	else if (whatToDo == "stats")
	{

		printf("%s.\n", GetPlayer(packet->systemAddress).m_name.c_str());
		for (auto it: m_players)
		{

			
			SPlayer& player = it.second;
			printf("%s. \n", player.m_name.c_str());
			if (player.m_health > 0)
			{
				player.SendStats(packet->systemAddress, false);
				continue;
			}
			
		}
		
	}
	else if (whatToDo == "toAttack")
	{

		for (std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		{

			SPlayer& player = it->second;
			if (player.m_health > 0)
			{
				bsWrite.Write((RakNet::MessageID)ID_WHOTOATTACK);
				RakNet::RakString playerName(player.m_name.c_str());
				bsWrite.Write(playerName);
				assert(g_rakPeerInterface->Send(&bsWrite, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
			}

		}
	}

	//End Condition
	if (peopleAlive <= 1)
	{
		for (std::map<RakNet::SystemAddress, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
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

void InputHandler()
{
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
			bs.Write((RakNet::MessageID)ID_PLAYER_CHARACTER);
			RakNet::RakString character(userInput);
			bs.Write(character);

			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_Pending)
		{
			static bool doOnce = false;
			if (!doOnce)
				std::cout << "pending..." << std::endl;

			doOnce = true;
		}
		else if (g_networkState == NS_InGame)
		{
			std::cout << "you can type stats to get how many people are alive or just talk to the other players." << std::endl;
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
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
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
				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;
				case ID_PLAYER_CHARACTER:
					PlayerCharacterSelect(packet);
					break;
				case ID_PLAYER_CLASSPICKED:
					DisplayPickedClass(packet);
					break;
				case ID_START_GAME:
					StartGame(packet);
					break;
				case ID_WHOS_TURN:
					DisplayActivePlayer(packet);
					break;
				case ID_PLAYERSTATS:
					GetPlayerAlive(packet);
					break;
				case ID_WHOTOATTACK:
					Targets(packet);
					break;
				case ID_ATTACK:
					AttackedTarget(packet);
					break;
				case ID_PLAYERTARGET:
					PlayerHasAttack(packet);
					break;
				case ID_THEGAME_START:
					GameManager(packet);
					break;
				case ID_WINNER:
					DisplayWinner(packet);
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
	int timer = 5;
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