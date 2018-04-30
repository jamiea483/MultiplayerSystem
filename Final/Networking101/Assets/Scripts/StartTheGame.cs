using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;

public class StartTheGame : NetworkBehaviour {

	public static bool m_gameStarted = false;
	[SyncVar]
	public bool m_ready = false;
	public Text m_readyText = null;
	public CTFGameManager manager;

	// Use this for initialization
	void Start () {
		if (isLocalPlayer) {
			m_readyText = FindObjectOfType<CTFGameManager> ().m_ready;
			manager = FindObjectOfType<CTFGameManager> ();
			//m_readyText.text = " Not ready";
		}
	}
	
	// Update is called once per frame
	void Update () {
		if (!isLocalPlayer) {
			return;
		}

		if (manager.GetState() == CTFGameManager.CTF_GameState.GS_Ready) {
			if (Input.GetKeyDown (KeyCode.Space)) {
				CmdReadyUp ();
			}

			if (m_ready) {
				m_readyText.text = " Press Spaceebar get ready.";
			} else {
				m_readyText.text = " Press spacebar to get unready.";
			}
		}
	}

	[Command]
	public void CmdReadyUp(){
		Ready ();

	}

	[ClientRpc]
	public void RpcReadyUp(){
		Ready ();
	}

	public void Ready(){
		if (!m_ready) {
			m_ready = true;
		} else {
			m_ready = false;
		}
	}
}
