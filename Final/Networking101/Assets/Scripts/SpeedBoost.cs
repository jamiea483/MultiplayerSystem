using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;

public class SpeedBoost : NetworkBehaviour {



	// Use this for initialization
	void Start () {
		
	}
	
	void OnCollisionEnter(Collision other){
		if (other.gameObject.tag == "Player") {
			ApplySpeed (other.gameObject);
		}
	}

	[Command]
	void CmdApplySpeed(GameObject player){
		ApplySpeed (player);
		RpcApplySpeed(player);
	}

	[ClientRpc]
	void RpcApplySpeed(GameObject player)
	{
		ApplySpeed (player);
	}

	void ApplySpeed(GameObject player){
		player.GetComponent<PlayerController> ().m_speedUp = true;
		Destroy (this.gameObject);
	}

}
