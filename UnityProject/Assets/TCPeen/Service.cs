using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace TCPeen
{
    public class Service
    {
        private bool _initialized;
        private bool _started;

        public Service()
        {
        }

        public bool Setup(int maxClients)
        {
            if (_initialized) return false;
            if (!Native.ServiceSetup(maxClients)) return false;
            _initialized = true;
            return true;
        }

        public bool Cleanup()
        {
            if (!_initialized || _started) return false;
            if (!Native.ServiceCleanup()) return false;
            _initialized = false;
            return true;
        }

        public bool Start(string ipstr, int port)
        {
            if (!_initialized || _started) return false;
            if (!Native.ServiceStart(ipstr, port)) return false;
            _started = true;
            return true;
        }

        public bool Stop()
        {
            if (!_initialized || !_started) return false;
            if (!Native.ServiceStop()) return false;
            _started = false;
            return true;
        }

        public bool Update()
        {
            if (!_initialized || !_started) return false;
            return Native.ServiceUpdate();
        }
    }
}
