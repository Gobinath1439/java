﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using CGSFNETCommon;

namespace ChatServer1
{
    public partial class MainForm : Form
    {
        System.Windows.Threading.DispatcherTimer workProcessTimer = new System.Windows.Threading.DispatcherTimer();

        bool IsStartServerNetwork = false;
        ServerNetwork ServerNet = new ServerNetwork();
        
        CgsfNET64Lib.NetworkConfig Config;

        // 최대 로비 개수
        int MaxLobbyCount = 0;

        // 로비 당 최대 유저 수
        int MaxLobbyUserCount = 0;
        
        List<int> SessionList = new List<int>();

        // 패킷 핸들러 매니져
        PktHandlerManager HandelrMgr = new PktHandlerManager();

        

        public MainForm()
        {
            InitializeComponent();
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            workProcessTimer.Tick += new EventHandler(OnProcessTimedEvent);
            workProcessTimer.Interval = new TimeSpan(0, 0, 0, 0, 1);
            workProcessTimer.Start();

            Config = new CgsfNET64Lib.NetworkConfig()
            {
                IP = Properties.Settings.Default.IP,
                Port = Properties.Settings.Default.Port,
                EngineDllName = Properties.Settings.Default.EngineDllName,
                MaxAcceptCount = Properties.Settings.Default.MaxAcceptCount,
                ThreadCount = Properties.Settings.Default.ThreadCount,
                ProtocolOption = 0,
                ProtocolID = 0,
                MaxBufferSize = Properties.Settings.Default.MaxBufferSize,
                MaxPacketSize = Properties.Settings.Default.MaxPacketSize,
            };

            MaxLobbyCount = Properties.Settings.Default.MaxLobbyCount;
            MaxLobbyUserCount = Properties.Settings.Default.MaxLobbyUserCount;


            var result = ServerNet.Init(Config, null, null);
            if (result != CgsfNET64Lib.NET_ERROR_CODE_N.SUCCESS)
            {
                DevLog.Write(string.Format("[Init] 네트워크 라이브러리 초기화 실패. {0}, {1}", result.ToString(), result), LOG_LEVEL.ERROR);
                return;
            }

            IsStartServerNetwork = true;
            if (ServerNet.Start(Config.ProtocolID))
            {
                DevLog.Write(string.Format("[Start] 네트워크 시작"), LOG_LEVEL.INFO);
            }
            else
            {
                DevLog.Write(string.Format("[Start] 네트워크 시작 실패"), LOG_LEVEL.ERROR);
            }


            HandelrMgr.Create(ServerNet, MaxLobbyCount, MaxLobbyUserCount);


            SetGUIInfo();
            ConnectCountToGUI(0);
        }

        private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (IsStartServerNetwork)
            {
                DevLog.Write(string.Format("[Stop] 네트워크 종료"), LOG_LEVEL.INFO);
                ServerNet.Stop();
            }
        }

        // 서버 정로를 UI에 출력
        void SetGUIInfo()
        {
            textBoxServerConfig.AppendText(string.Format("IP:{0}, Port:{1}, EngineDllName:{2} {3}", Config.IP, Config.Port, Config.EngineDllName, Environment.NewLine));
            textBoxServerConfig.AppendText(string.Format("ThreadCount:{0}, MaxBufferSize:{1}, MaxPacketSize:{2} {3}", Config.ThreadCount, Config.MaxBufferSize, Config.MaxPacketSize, Environment.NewLine));

            for(int i = 0; i < MaxLobbyCount; ++i)
            {
                ListViewItem lvi = new ListViewItem((i+1).ToString());
                lvi.SubItems.Add(0.ToString());
                listViewLobbyInfo.Items.Add(lvi);
            }

            listViewLobbyInfo.Refresh();
        }

        // 현재 접속 인원 수를 UI에 출력
        void ConnectCountToGUI(int count)
        {
            textBoxConnectCount.Text = count.ToString();
        }
                       
        private void OnProcessTimedEvent(object sender, EventArgs e)
        {
            try
            {
                ProcessProcket();
                ProcessInnerMessage();
                ProcessLog();
            }
            catch (Exception ex)
            {
                DevLog.Write(string.Format("[OnProcessTimedEvent] Exception:{0}", ex.ToString()), LOG_LEVEL.ERROR);
            }
        }

        private void ProcessProcket()
        {
            var packet = ServerNet.GetPacket();
            if (packet == null)
            {
                return;
            }

            switch (packet.GetPacketType())
            {
                case CgsfNET64Lib.SFPACKET_TYPE.CONNECT:
                    SessionList.Add(packet.SessionID());
                    ConnectCountToGUI(SessionList.Count());
                    DevLog.Write(string.Format("[OnConnect] SessionID:{0}", packet.SessionID()), LOG_LEVEL.INFO);
                    break;
                case CgsfNET64Lib.SFPACKET_TYPE.DISCONNECT:
                    SessionList.Remove(packet.SessionID());
                    HandelrMgr.ClientDisConnect(packet.SessionID());
                    ConnectCountToGUI(SessionList.Count());
                    DevLog.Write(string.Format("[OnDisConnect] SessionID:{0}", packet.SessionID()), LOG_LEVEL.INFO);
                    break;
                case CgsfNET64Lib.SFPACKET_TYPE.DATA:
                    HandelrMgr.Process(packet);
                    break;
            }
        }

        void ProcessInnerMessage()
        {
            while (true)
            {
                InnerMsg msg;

                if (InnerMessageQueue.GetMsg(out msg))
                {
                    switch (msg.Type)
                    {
                        case InnerMsgType.CURRENT_LOBBY_USER_COUNT:
                            {
                                var tokens = msg.Value1.Split("_");

                                int iItemNum = listViewLobbyInfo.Items.Count;

                                for (int i = 0; i < iItemNum; ++i)
                                {
                                    if (tokens[0] == listViewLobbyInfo.Items[i].SubItems[0].Text)
                                    {
                                        listViewLobbyInfo.Items[i].SubItems[1].Text = tokens[1];
                                        listViewLobbyInfo.Refresh();
                                        return;
                                    }
                                }
                            }
                            break;
                                                   
                    }
                }
                else
                {
                    break;
                }
            }
        }

        private void ProcessLog()
        {
            // 너무 이 작업만 할 수 없으므로 일정 작업 이상을 하면 일단 패스한다.
            int logWorkCount = 0;

            while (true)
            {
                string msg;

                if (DevLog.GetLog(out msg))
                {
                    ++logWorkCount;

                    if (listBoxLog.Items.Count > 512)
                    {
                        listBoxLog.Items.Clear();
                    }

                    listBoxLog.Items.Add(msg);
                    listBoxLog.SelectedIndex = listBoxLog.Items.Count - 1;
                }
                else
                {
                    break;
                }

                if (logWorkCount > 32)
                {
                    break;
                }
            }
        }


        

        
    }
}
