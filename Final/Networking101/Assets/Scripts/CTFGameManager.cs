using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;

public class CTFGameManager : NetworkBehaviour {

    public int m_numPlayers = 3;
    public float m_gameTime = 5.0f;
    public GameObject m_flag = null;
	public GameObject m_powerUp = null;
	public StartTheGame[] m_players;
	public Text m_ready = null;
	public Text player1 = null;
	public Text player2 = null;
	public Text player3 = null;
	[SyncVar]
	public int m_playerTag = 0;
	[SyncVar]
	public int player1Score = 0;
	[SyncVar]
	public int player2Score = 0;
	[SyncVar]
	public int player3Score = 0;

	private Timer m_time;
	private GameObject m_powerOnScreen = null;
	private float m_powerTimer = 10.0f;
	private bool doOnce = false;
	private float m_elapseTime = 0;

    public enum CTF_GameState
    {
        GS_WaitingForPlayers,
        GS_Ready,
        GS_InGame,
		GS_EndGame,
    }

    [SyncVar]
    CTF_GameState m_gameState = CTF_GameState.GS_WaitingForPlayers;

    public bool SpawnFlag()
    {
		Vector3 spawnPoint;
		ObjectSpawner.RandomPoint(new Vector3(0, 0, 0), 10.0f, out spawnPoint);
		GameObject flag = Instantiate(m_flag, spawnPoint, new Quaternion());
        NetworkServer.Spawn(flag);
        return true;
    }

	public bool SpawnPower()
	{
		Vector3 spawnPoint;
		ObjectSpawner.RandomPoint(new Vector3(0, 0, 0), 10.0f, out spawnPoint);
		m_powerOnScreen = Instantiate(m_powerUp, spawnPoint, new Quaternion());
		NetworkServer.Spawn(m_powerOnScreen);
		return true;
	}

    bool IsNumPlayersReached()
    {
        return CTFNetworkManager.singleton.numPlayers == m_numPlayers;
    }

	public CTF_GameState GetState(){
		return m_gameState;
	}
	// Use this for initialization
	void Start () {
		m_time = FindObjectOfType<Timer> ();
    }
	
	// Update is called once per frame
	void Update ()
    {
	    if(isServer)
        {
            if(m_gameState == CTF_GameState.GS_WaitingForPlayers && IsNumPlayersReached())
            {
                m_gameState = CTF_GameState.GS_Ready;
				m_players = FindObjectsOfType<StartTheGame> ();
            }
        }

        UpdateGameState();
	}

    public void UpdateGameState()
    {
      if (m_gameState == CTF_GameState.GS_Ready)
        {
			int playerReady = 0;
            //call whatever needs to be called
            if (isServer)
            {
				Debug.Log ("Tootal Players: " + m_players.Length);

				//checks if the all the players are ready 
				for(int i = 0; i < m_players.Length; i++){
					if (m_players [i].m_ready) {
						playerReady++;
						Debug.Log ("Num of players ready: " + playerReady);
					}
				}

				if (playerReady == m_numPlayers) {
					SpawnFlag ();
					m_gameState = CTF_GameState.GS_InGame;
					Timer.m_startTime = true;
					StartTheGame.m_gameStarted = true;

				}

            }
        }
		if (m_gameState == CTF_GameState.GS_InGame) {
			m_ready.text = "";
			if (isServer) {
				Game ();


			}
		}
		if (m_gameState == CTF_GameState.GS_EndGame) {
			m_ready.text = "Game Over";
		}
        
    }

	//Timer for the powerup spawn
	void Game()
	{
		RpcScore ();

		for (int i = 0; i < m_players.Length; i++) {
			if (m_players [i].GetComponent<PlayerController> ().HasFlag ()) {
				m_elapseTime += Time.deltaTime;
				if (m_elapseTime >= 1) {
					m_elapseTime = 0;
					if (i == 0)
						player1Score++;
					else if (i == 1)
						player2Score++;
					else
						player3Score++;
				}
			}
			if (m_time.m_timeLeft <= 0) {
				m_gameState = CTF_GameState.GS_EndGame;
			}
		}

		if (!m_powerOnScreen) {
			m_powerTimer -= Time.deltaTime;
			if (m_powerTimer <= 0) {
				SpawnPower ();
				m_powerTimer = 10;
			}
		}
	}

	[ClientRpc]
	public void RpcScore(){
		SetScoreScore();
	}

	public void SetScoreScore(){
		player1.text = "Player1: " + player1Score;
		player2.text = "Player2: " + player2Score;
		player3.text = "Player3: " + player3Score;

	}
}
