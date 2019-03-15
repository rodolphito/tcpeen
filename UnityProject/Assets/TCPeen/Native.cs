using System.Runtime.InteropServices;
using System;
using System.IO;
using System.Diagnostics;
using System.Security;
using UnityEngine;


namespace TCPeen
{

    public static unsafe class Native
    {
        // tcp_service_state_t: service state
        enum ServiceState
        {
            TCP_SERVICE_NEW,
            TCP_SERVICE_STARTING,
            TCP_SERVICE_STARTED,
            TCP_SERVICE_STOPPING,
            TCP_SERVICE_STOPPED,
            TCP_SERVICE_ERROR,
            TCP_SERVICE_INVALID,
        };

        // hb_event_type_t: event type
        enum EventType
        {
            HB_EVENT_NONE = 0,
            HB_EVENT_ERROR,
            HB_EVENT_CLIENT_OPEN,
            HB_EVENT_CLIENT_CLOSE,
            HB_EVENT_CLIENT_READ,
        };

        // hb_event_base_t: base event
        [StructLayout(LayoutKind.Explicit, Size = 256, CharSet = CharSet.Ansi)]
        internal ref struct EventBase
        {
            [FieldOffset(0)] public UInt32 id;
            [FieldOffset(4)] public UInt32 type;
        };

        // hb_event_error_t: error event
        [StructLayout(LayoutKind.Explicit, Size = 256, CharSet = CharSet.Ansi)]
        internal ref struct EventError
        {
            [FieldOffset(0)] public UInt32 id;
            [FieldOffset(4)] public UInt32 type;
            [FieldOffset(8)] public UInt64 client_id;
            [FieldOffset(24)] public UInt32 error_code;
            [FieldOffset(28)] public fixed byte error_string[228];
        };

        // hb_event_client_open_t: client connected
        [StructLayout(LayoutKind.Explicit, Size = 256, CharSet = CharSet.Ansi)]
        internal ref struct EventClientOpen
        {
            [FieldOffset(0)] public UInt32 id;
            [FieldOffset(4)] public UInt32 type;
            [FieldOffset(8)] public UInt64 client_id;
            [FieldOffset(16)] public fixed byte host_local[64];
            [FieldOffset(80)] public fixed byte host_remote[64];
        };

        // hb_event_client_close_t: client disconnected
        [StructLayout(LayoutKind.Explicit, Size = 256, CharSet = CharSet.Ansi)]
        internal ref struct EventClientClose
        {
            [FieldOffset(0)] public UInt32 id;
            [FieldOffset(4)] public UInt32 type;
            [FieldOffset(8)] public UInt64 client_id;
            [FieldOffset(24)] public UInt32 error_code;
            [FieldOffset(28)] public fixed byte error_string[228];
        };

        // hb_event_client_read_t: client recv buffer
        [StructLayout(LayoutKind.Explicit, Size = 256, CharSet = CharSet.Ansi)]
        internal ref struct EventClientRead
        {
            [FieldOffset(0)] public UInt32 id;
            [FieldOffset(4)] public UInt32 type;
            [FieldOffset(8)] public UInt64 client_id;
            [FieldOffset(16)] public byte* buffer;
            [FieldOffset(24)] public UInt64 len;
        };

        [DllImport("tcpeen_unity"), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_setup(UInt64 max_clients);

        [DllImport("tcpeen_unity"), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_cleanup();

        [DllImport("tcpeen_unity", CharSet = CharSet.Ansi), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_start([MarshalAs(UnmanagedType.LPStr)] string ipstr, UInt16 port);

        [DllImport("tcpeen_unity"), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_stop();

        [DllImport("tcpeen_unity"), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_events_acquire(EventBase ***out_evt_ptr_arr, UInt64 *out_evt_ptr_count);

        [DllImport("tcpeen_unity"), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_events_release();

        [DllImport("tcpeen_unity"), SuppressUnmanagedCodeSecurity]
        private static extern int tcpeen_service_send(UInt64 client_id, byte *buffer, UInt64 length);

        public static bool ServiceSetup(int maxClients)
        {
            if (tcpeen_service_setup((UInt64)maxClients) != 0) return false;
            return true;
        }

        public static bool ServiceCleanup()
        {
            if (tcpeen_service_cleanup() != 0) return false;
            return true;
        }

        public static bool ServiceStart(string ipstr, int port)
        {
            if (tcpeen_service_start(ipstr, (UInt16)port) != 0) return false;
            return true;
        }

        public static bool ServiceStop()
        {
            if (tcpeen_service_stop() != 0) return false;
            return true;
        }

        public static bool ServiceUpdate()
        {
            EventBase **evtPtr;
            UInt64 evtCount = 0;

            if (tcpeen_service_events_acquire(&evtPtr, &evtCount) != 0) return false;

            for (UInt64 e = 0; e < evtCount; e++)
            {
                switch ((EventType)evtPtr[e]->type)
                {
                case EventType.HB_EVENT_CLIENT_OPEN:
                    EventClientOpen *evtOpen = (EventClientOpen *)evtPtr[e];
                    //UnityEngine.Debug.LogFormat("Connected Event -- ID# {0}", evtOpen->client_id);
                    //UnityEngine.Debug.LogFormat("Endpoints: {0} -> {1}", Marshal.PtrToStringAnsi((IntPtr)evtOpen->host_remote), Marshal.PtrToStringAnsi((IntPtr)evtOpen->host_local));
                    break;
                case EventType.HB_EVENT_CLIENT_CLOSE:
                    EventClientClose *evtClose = (EventClientClose*)evtPtr[e];
                    //UnityEngine.Debug.LogFormat("Disconnected Event -- ID# {0}", evtClose->client_id);
                    break;
                case EventType.HB_EVENT_CLIENT_READ:
                    EventClientRead *evtRead = (EventClientRead *)evtPtr[e];
                    //IntPtr buffer = (IntPtr)evtRead->buffer;
                    //UnityEngine.Debug.LogFormat("Receive Event -- ID# {0}", evtRead->client_id);
                    //UnityEngine.Debug.LogFormat("Received {0} bytes: {1}", evtRead->length, Marshal.PtrToStringAnsi(IntPtr.Add(buffer, 4)));

                    if (tcpeen_service_send(evtRead->client_id, evtRead->buffer, evtRead->len) != 0)
                    {
                        UnityEngine.Debug.LogError("Send failed for client id: " + evtRead->client_id);
                    }

                    break;
                }
            }


            if (tcpeen_service_events_release() != 0)
            {
                UnityEngine.Debug.LogError("Failed to release native events");
                return false;
            }

            return true;
        }
    }
}