using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class StartupBehaviour : MonoBehaviour
{
    private TCPeen.Service _tcpServ;
    private void Start()
    {
        _tcpServ = new TCPeen.Service();

        // this is temporarily hardcoded in native
        if (!_tcpServ.Setup(10000))
        {
            Debug.LogError("Service Setup failed");
            return;
        }

        if (!_tcpServ.Start("0.0.0.0", 7777))
        {
            Debug.Log("Service Start failed");
        }
    }

    private void OnDestroy()
    {
        if (!_tcpServ.Stop())
        {
            Debug.Log("ServiceStop failed");
        }

        if (!_tcpServ.Cleanup())
        {
            Debug.LogError("ServiceCleanup failed");
        }
    }

    private void Update()
    {
        _tcpServ.Update();
    }
}
