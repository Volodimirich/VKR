        /* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 *
 * Two hosts connected to different OpenFlow switches.
 * Both switches are managed by the same external controller application.
 *
 *                        External Controller
 *                                |
 *                         +-------------+
 *                         |             |
 *                  +----------+     +----------+
 *       Host 0 === | Switch 0 | === | Switch 1 | === Host 1
 *                  +----------+     +----------+
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include <unistd.h>


using namespace ns3;
CsmaHelper csmaHelper;
int start = clock();
std::ofstream out;


int get_packet_time(Packet pack) {
    size_t size = 4;
    float time;
    uint8_t *out_data =  static_cast<uint8_t *>(malloc(size));
    pack.CopyData(out_data, size);
    time = ((out_data[3] << 24) | (out_data[2] << 16) | (out_data[1] << 8) | out_data[0]);
//    time = (float)((out_data[3] << 24) | (out_data[2] << 16) | (out_data[1] << 8) | out_data[0]) / CLOCKS_PER_SEC;
    free(out_data);
    return time;
}

Ptr<Packet> create_packet() {
    size_t size = 4;
    uint8_t *data = static_cast<uint8_t *>(malloc(size));
    int *data_int = (int *)data;
    data_int[0] = clock() - start;
    return Create<Packet> (data, size);
}

void dstSocketRecv (Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);
    float time = get_packet_time(*packet);
    out << "Sending time - <" <<  time << "> Current time - <" << clock() << ">" << std::endl;
    packet->RemoveAllPacketTags ();
    packet->RemoveAllByteTags ();
    InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
    NS_LOG_INFO ("Destination Received " << packet->GetSize () << " bytes from " << address.GetIpv4 ());
//    SendStuff (socket, Ipv4Address ("10.10.1.2"), address.GetPort ());
}


void SendStuff (Ptr<Socket> sock, Ipv4Address dstaddr, uint16_t port) {
    Ptr <Packet> p = create_packet();
    p->AddPaddingAtEnd(100);
    sock->SendTo(p, 0, InetSocketAddress(dstaddr, port));
    return;
}

void EndSimulation() {
    Simulator::Stop();
}

void SwitchInstall(std::pair<size_t, size_t> indexes, NetDeviceContainer* switchPorts, NodeContainer switches) {
    NodeContainer pair = NodeContainer (switches.Get (indexes.first), switches.Get (indexes.second));
    NetDeviceContainer pairDevs = csmaHelper.Install (pair);
    switchPorts [indexes.first].Add (pairDevs.Get (0));
    switchPorts [indexes.second].Add (pairDevs.Get (1));
}



/** Controller 0 installs the rule to forward packets from host 0 to 1 (port 1 to 2). */
class Controller : public OFSwitch13Controller
{
public:
    void FuckingSlave (uint64_t swtch);

protected:
    void HandshakeSuccessful (Ptr<const RemoteSwitch> swtch);

};

void
Controller::HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
{
    DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=2 in_port=1 write:output=2");
    DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=2 in_port=2 write:output=1");
}



void
Controller::FuckingSlave (uint64_t swtch)
{
    DpctlExecute (swtch, "flow-mod cmd=del,table=0,prio=1 in_port=2 write:output=1");
}


void OpenFlowCommandRule(uint64_t dpid,  Ptr<Controller> ctrl ) {
    ctrl->FuckingSlave(dpid);
//    DpctlExecute (dpid, "flow-mod cmd=add,table=0,prio=700 "
//                        "in_port=4,eth_type=0x0800,ip_proto=6 apply:group=3");
}





int main (int argc, char *argv[])
{
    float interval = 0.1;
    float time = 0.1;
    float rec_time = 10;
    int delay = 2;
    std::string daterate = "100Mbps";


  // Configure command line parameters
    CommandLine cmd;
    out.open("data.txt"); // окрываем файл для записи

    cmd.AddValue ("daterate", "Csma daterate", daterate);
    cmd.AddValue ("delay", "Csma delay(MilliSeconds)", delay);
    cmd.AddValue ("time_between_packages", "Time between sending packets", interval);
    cmd.AddValue ("reconfiguration", "Reconfiguration time", rec_time);
    cmd.Parse (argc, argv);
    
    out << "Csma daterate - " <<  daterate << std::endl;
    out << "Csma delay - " <<  delay << std::endl;
    out << "Time between sending packets - " <<  interval << std::endl;
    out << "Reconfiguration time - " <<  rec_time << std::endl;

    // Enable checksum computations (required by OFSwitch13 module)
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    // Create two host nodes
    Ptr<Node> PCLeft1 = CreateObject<Node> ();
    Ptr<Node> PCRight1 = CreateObject<Node> ();
    NodeContainer hosts = NodeContainer(PCLeft1, PCRight1);

    // Create two switch nodes
    NodeContainer switches;

    switches.Create (4);

    // Use the CsmaHelper to connect hosts and switches
//    CsmaHelper csmaHelper;
//    csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate (daterate)));
    csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));

    NodeContainer pair;
    NetDeviceContainer pairDevs;
    NetDeviceContainer hostDevices;

    NetDeviceContainer switchPorts [4];

    for (size_t i=0; i<4; i++)
        switchPorts [i] = NetDeviceContainer ();

    pair = NodeContainer(hosts.Get(0), switches.Get(0));
    pairDevs = csmaHelper.Install(pair);
    hostDevices.Add(pairDevs.Get(0));
    switchPorts[0].Add(pairDevs.Get(1));

    pair = NodeContainer(hosts.Get(1), switches.Get(3));
    pairDevs = csmaHelper.Install(pair);
    hostDevices.Add(pairDevs.Get(0));
    switchPorts[3].Add(pairDevs.Get(1));

    // Connect the switches
    SwitchInstall(std::make_pair(0, 1), switchPorts, switches);
    SwitchInstall(std::make_pair(1, 2), switchPorts, switches);
    SwitchInstall(std::make_pair(2, 3), switchPorts, switches);


    Ptr<Node> controllerNode = CreateObject<Node> ();
    Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
    Ptr<Controller> ctrl = CreateObject<Controller> ();

    of13Helper->InstallController (controllerNode, ctrl);
    of13Helper->InstallSwitch (switches.Get (0), switchPorts [0]);
    of13Helper->InstallSwitch (switches.Get (1), switchPorts [1]);
    of13Helper->InstallSwitch (switches.Get (2), switchPorts [2]);
    of13Helper->InstallSwitch (switches.Get (3), switchPorts [3]);
    of13Helper->CreateOpenFlowChannels ();



    // TapBridge the controller device to local machine
    // The default configuration expects a controller on local port 6653
    // Install the TCP/IP stack into hosts nodes
    InternetStackHelper internet;
    internet.Install (hosts);

    // Set IPv4 host addresses
    Ipv4AddressHelper ipv4helpr;
    Ipv4InterfaceContainer hostIpIfaces;
    ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
    hostIpIfaces = ipv4helpr.Assign (hostDevices);



    Ptr<Socket> srcSocket = Socket::CreateSocket (PCLeft1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    srcSocket->Bind ();
//
    Ptr<Socket> dstSocket = Socket::CreateSocket (PCRight1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint16_t dstport = 12345;
    Ipv4Address dstaddr = "10.1.1.2";
    InetSocketAddress dst = InetSocketAddress (dstaddr, dstport);
    dstSocket->Bind (dst);
    dstSocket->SetRecvCallback (MakeCallback (&dstSocketRecv));

    while (time < rec_time*1.5) {
        Simulator::Schedule (Seconds (time),&SendStuff, srcSocket, dstaddr, dstport);
        time += interval;
    }

    Simulator::Schedule (Seconds (rec_time), &OpenFlowCommandRule, 4, ctrl);

    Simulator::Schedule (Seconds (rec_time*2), &EndSimulation);
    Simulator::Run ();

    Simulator::Destroy ();

    return 0;
}
