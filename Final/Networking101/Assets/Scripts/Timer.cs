using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;

public class Timer : NetworkBehaviour {
	
	private float m_elapseTime = 0;
	[SyncVar(hook = "SetTime")]
	public float m_timeLeft = 60;
	public Text m_time = null;
	public static bool m_startTime = false;



	// Use this for initialization
	void Start () {
		m_time.text = "Time Left: " + m_timeLeft;
	}

	// Update is called once per frame
	void Update () {
		if (!isServer) 
		{
			return;
		}
			if (m_startTime) {
				m_elapseTime += Time.deltaTime;
				if (m_elapseTime >= 1.0f) {
				m_timeLeft -= 1.0f;
				m_elapseTime = 0;
				}
			if (m_timeLeft <= 0) {
				m_timeLeft = 0;
			}
			}
	}


	void SetTime(float m_timeLeft){
		m_time.text = "Time Left: " + m_timeLeft;
		m_elapseTime = 0;
	}
}
