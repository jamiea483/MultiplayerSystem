using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;

public class CustomMsgType
{
    public static short Transform = MsgType.Highest + 1;
};


public class PlayerController : NetworkBehaviour
{


    public float m_linearSpeed = 5.0f;
    public float m_angularSpeed = 3.0f;
    public float m_jumpSpeed = 5.0f;
	private float m_speedTime = 4;
    private Rigidbody m_rb = null;
	public bool m_speedUp = false;
	public CTFGameManager manager;

	[SyncVar]
	public int m_playerNum = 0;

    [SyncVar]
    public bool m_hasFlag = false;

	[SyncVar]
	public int score = 0;

    public bool HasFlag() {
        return m_hasFlag;
    }

    [Command]
    public void CmdPickUpFlag()
    {
        m_hasFlag = true;
    }

    [Command]
    public void CmdDropFlag()
    {
		DropFlag ();
		RpcDropflag ();
    }

	[ClientRpc]
	public void RpcDropflag(){
		DropFlag ();
	}

	public void DropFlag()
	{
		m_hasFlag = false;
	}

    bool IsHost()
    {
        return isServer && isLocalPlayer;
    }

    // Use this for initialization
    void Start() {


		manager = FindObjectOfType<CTFGameManager> ();

        m_rb = GetComponent<Rigidbody>();
        //Debug.Log("Start()");
        TrailRenderer tr = GetComponent<TrailRenderer>();
        tr.enabled = false;
    }

    public override void OnStartAuthority()
    {
        base.OnStartAuthority();
        //Debug.Log("OnStartAuthority()");
    }

    public override void OnStartClient()
    {
        base.OnStartClient();
        //Debug.Log("OnStartClient()");
    }

    public override void OnStartLocalPlayer()
    {
        base.OnStartLocalPlayer();
        //Debug.Log("OnStartLocalPlayer()");
        GetComponent<MeshRenderer>().material.color = new Color(0.0f, 1.0f, 0.0f);
    }

    public override void OnStartServer()
    {
        base.OnStartServer();
        //Debug.Log("OnStartServer()");
    }

    public void Jump()
    {
        Vector3 jumpVelocity = Vector3.up * m_jumpSpeed;
        m_rb.velocity += jumpVelocity;
        TrailRenderer tr = GetComponent<TrailRenderer>();
        tr.enabled = true;
    }

    [ClientRpc]
    public void RpcJump()
    {
        Jump();
    }

    [Command]
    public void CmdJump()
    {
        Jump();
        RpcJump();
    }

    // Update is called once per frame
    void Update() {

        if (!isLocalPlayer)
        {
            return;
        }


		if (manager.GetState() == CTFGameManager.CTF_GameState.GS_InGame) {
			if (m_rb.velocity.y < Mathf.Epsilon) {
				TrailRenderer tr = GetComponent<TrailRenderer> ();
				tr.enabled = false;
			}
			Vector3 linearVelocity;

			float rotationInput = Input.GetAxis ("Horizontal");
			float forwardInput = Input.GetAxis ("Vertical");
			if(!m_speedUp)
			linearVelocity = this.transform.forward * (forwardInput * m_linearSpeed);
			else 
				linearVelocity = this.transform.forward * (forwardInput * (2.0f *m_linearSpeed));
				
			if (Input.GetKeyDown (KeyCode.Space) && !HasFlag()) {
				CmdJump ();
			}

			float yVelocity = m_rb.velocity.y;


			linearVelocity.y = yVelocity;
			m_rb.velocity = linearVelocity;

			Vector3 angularVelocity = this.transform.up * (rotationInput * m_angularSpeed);
			m_rb.angularVelocity = angularVelocity;
		}

		// false when power up runs out
		if (m_speedUp) {
			m_speedTime -= Time.deltaTime;

			if (m_speedTime <= 0) {
				m_speedUp = false;
				m_speedTime = 4;
			}
		}
    }

    [Command]
    public void CmdPlayerDropFlag()
    {
        Transform childTran = this.transform.GetChild(this.transform.childCount - 1);
        Flag flag = childTran.gameObject.GetComponent<Flag>();
		if (flag) {
			m_hasFlag = false;
			flag.RpcDropFlag();
			flag.Drop ();
		}
    }

	//drops flag if it is touched by antother player
	public void OnCollisionEnter(Collision other)
	{
		if(!isLocalPlayer || other.collider.tag != "Player")
		{
			return;
		}
			
		if (HasFlag()) {
			Transform childTran = this.transform.GetChild (this.transform.childCount - 1);
			if (childTran.gameObject.tag == "Flag") {
                CmdPlayerDropFlag();
            }

		}
	}
}
