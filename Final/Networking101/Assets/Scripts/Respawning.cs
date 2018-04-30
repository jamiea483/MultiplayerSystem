using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;

public class Respawning : NetworkBehaviour {
	private NetworkStartPosition[] spawnPoints;


	// Use this for initialization
	void Start () {
		if (isLocalPlayer) {
			spawnPoints	= FindObjectsOfType<NetworkStartPosition> ();
		}
	}
	
	// Update is called once per frame
	void Update () {
		if (!isServer) {
			return;
		}
	}

	[ClientRpc]
	public void RpcRespawn(){
		Respawn ();
	}

	[Command]
	public void CmdRespawn(){
		Respawn ();
		RpcRespawn ();
	}

	public void Respawn(){

		if (isLocalPlayer) {
			Vector3 spawnPoint = Vector3.zero;

			if (spawnPoints != null && spawnPoints.Length > 0) {
				spawnPoint = spawnPoints [Random.Range (0, spawnPoints.Length)].transform.position;
			}
			transform.position = spawnPoint;
		}
	}

	void OnCollisionEnter(Collision other){
		if (!isServer) {
			return;
		}

		if (other.gameObject.tag == "Player") {
			RpcRespawn ();
		}
	}
}
