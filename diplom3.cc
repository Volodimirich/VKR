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
clock_t start = clock();
std::ofstream out;
int counter1 = 0;
int counter2 = 0;


std::tuple<int, int, int> get_packet_time(Packet pack) {
    size_t size = 12;
    int time;
    int packet_name;
    int from;
    uint8_t *out_data =  static_cast<uint8_t *>(malloc(size));
    pack.CopyData(out_data, size);
    time = ((out_data[3] << 24) | (out_data[2] << 16) | (out_data[1] << 8) | out_data[0]);
    packet_name = ((out_data[7] << 24) | (out_data[6] << 16) | (out_data[5] << 8) | out_data[4]);
    from = ((out_data[11] << 24) | (out_data[10] << 16) | (out_data[9] << 8) | out_data[8]);
//    time = ((float)((out_data[3] << 24) | (out_data[2] << 16) | (out_data[1] << 8) | out_data[0]) )/ CLOCKS_PER_SEC;
    free(out_data);
    return std::make_tuple(time, packet_name, from);
}

Ptr<Packet> create_packet(int from) {
    size_t size = 12;
    int ct;
    if (from == 0) {
        ct = counter1;
        counter1++;
    }  else {
        ct = counter2;
        counter2++;
    }
    uint8_t *data = static_cast<uint8_t *>(malloc(size));
    int *data_int = (int *)data;
    data_int[0] = clock() - start;
    data_int[1] = ct;
    data_int[2] = from;

    return Create<Packet> (data, size);
}

void dstSocketRecv (Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);
    int time, counter, send_from;
    std::tie(time, counter, send_from) = get_packet_time(*packet);
//    out << "Sending time - <" <<  time << "> Current time - <" << clock() << ">" << std::endl;
    std::cout << "Sending time - <" <<  (time/(double) CLOCKS_PER_SEC) << "> Current time - <" << (clock()/(double) CLOCKS_PER_SEC) << "> Packet name - <" << counter << "> Sended from - <" << send_from << ">" << std::endl;
    out << "Sending time - <" <<  (time/(double) CLOCKS_PER_SEC) << "> Current time - <" << (clock()/(double) CLOCKS_PER_SEC) << "> Packet name - <" << counter << "> Sended from - <" << send_from << ">" << std::endl;
    packet->RemoveAllPacketTags ();
    packet->RemoveAllByteTags ();
    InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
    NS_LOG_INFO ("Destination Received " << packet->GetSize () << " bytes from " << address.GetIpv4 ());
//    SendStuff (socket, Ipv4Address ("10.10.1.2"), address.GetPort ());
}


void SendStuff (Ptr<Socket> sock, Ipv4Address dstaddr, uint16_t port, int from) {
    Ptr <Packet> p = create_packet(from);
    p->AddPaddingAtEnd(100);
    std::cout << "Send to " << dstaddr << std::endl;
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
    void BadReconf (uint64_t swtch);

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
Controller::BadReconf (uint64_t swtch)
{
    DpctlExecute (swtch, "flow-mod cmd=del,table=0,prio=1 in_port=2 write:output=1");
}


void OpenFlowCommandRule(uint64_t dpid,  Ptr<Controller> ctrl ) {
    ctrl->BadReconf(dpid);
//    DpctlExecute (dpid, "flow-mod cmd=add,table=0,prio=700 "
//                        "in_port=4,eth_type=0x0800,ip_proto=6 apply:group=3");
}





int main (int argc, char *argv[])
{
    float interval = 0.1;
    float time = 0;
    float rec_time = 10;
    int delay = 2;
    int bad_rec;
    std::string daterate = "100Mbps";


  // Configure command line parameters
    CommandLine cmd;
    out.open("data.txt"); // окрываем файл для записи

    cmd.AddValue ("daterate", "Csma daterate", daterate);
    cmd.AddValue ("delay", "Csma delay(MilliSeconds)", delay);
    cmd.AddValue ("time_between_packages", "Time between sending packets", interval);
    cmd.AddValue ("reconfiguration", "Reconfiguration time", rec_time);
    cmd.AddValue ("bad_reconf_on", "Where in bad reconfiguration? 3-everythere, 2-Upper, 1-Lower, 0-Nowhere", bad_rec);
    cmd.Parse (argc, argv);

    out << "Csma daterate - " <<  daterate << std::endl;
    out << "Csma delay - " <<  delay << std::endl;
    out << "Time between sending packets - " <<  interval << std::endl;
    out << "Reconfiguration time - " <<  rec_time << std::endl;

    // Enable checksum computations (required by OFSwitch13 module)
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    // Create two host nodesu
    Ptr<Node> PCLeft1 = CreateObject<Node> ();
    Ptr<Node> PCLeft2 = CreateObject<Node> ();

    Ptr<Node> PCMid1 = CreateObject<Node> ();
    Ptr<Node> PCMid2 = CreateObject<Node> ();

    Ptr<Node> PCRight1 = CreateObject<Node> ();
    NodeContainer hostsLow = NodeContainer(PCLeft1, PCMid1, PCRight1);
    NodeContainer hostsUp = NodeContainer(PCLeft2, PCMid2, PCRight1);
    NodeContainer hosts = NodeContainer(PCLeft1,  PCLeft2, PCMid1, PCMid2, PCRight1);

    // Create two switch nodes
    NodeContainer switchesLow;
    NodeContainer switchesUp;

    switchesLow.Create (4);
    switchesUp.Create (4);

    // Use the CsmaHelper to connect hosts and switches
//    CsmaHelper csmaHelper;
//    csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate (daterate)));
    csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));

    NodeContainer pair;
    NetDeviceContainer pairDevs;
    NetDeviceContainer hostDevicesLow;
    NetDeviceContainer hostDevicesUp;

    NetDeviceContainer switchPortsLow [4];
    NetDeviceContainer switchPortsUp [4];

    for (size_t i=0; i<4; i++) {
        switchPortsLow[i] = NetDeviceContainer();
        switchPortsUp[i] = NetDeviceContainer();
    }

    pair = NodeContainer(hostsLow.Get(0), switchesLow.Get(0));
    pairDevs = csmaHelper.Install(pair);
    hostDevicesLow.Add(pairDevs.Get(0));
    switchPortsLow[0].Add(pairDevs.Get(1));

    pair = NodeContainer(hostsUp.Get(0), switchesUp.Get(0));
    pairDevs = csmaHelper.Install(pair);
    hostDevicesUp.Add(pairDevs.Get(0));
    switchPortsUp[0].Add(pairDevs.Get(1));


    pair = NodeContainer(hostsLow.Get(2), switchesLow.Get(3));
    pairDevs = csmaHelper.Install(pair);
    hostDevicesLow.Add(pairDevs.Get(0));
    switchPortsLow[3].Add(pairDevs.Get(1));


    pair = NodeContainer(hostsUp.Get(2), switchesUp.Get(3));
    pairDevs = csmaHelper.Install(pair);
    hostDevicesUp.Add(pairDevs.Get(0));
    switchPortsUp[3].Add(pairDevs.Get(1));

    // Connect the switches
    SwitchInstall(std::make_pair(0, 1), switchPortsLow, switchesLow);
    SwitchInstall(std::make_pair(1, 2), switchPortsLow, switchesLow);
    SwitchInstall(std::make_pair(2, 3), switchPortsLow, switchesLow);

    SwitchInstall(std::make_pair(0, 1), switchPortsUp, switchesUp);
    SwitchInstall(std::make_pair(1, 2), switchPortsUp, switchesUp);
    SwitchInstall(std::make_pair(2, 3), switchPortsUp, switchesUp);

    pair = NodeContainer(hostsLow.Get(1), switchesLow.Get(1));
    pairDevs = csmaHelper.Install(pair);
    hostDevicesLow.Add(pairDevs.Get(0));
    switchPortsLow[1].Add(pairDevs.Get(1));


    pair = NodeContainer(hostsUp.Get(1), switchesUp.Get(1));
    pairDevs = csmaHelper.Install(pair);
    hostDevicesUp.Add(pairDevs.Get(0));
    switchPortsUp[1].Add(pairDevs.Get(1));


    Ptr<Node> controllerNode = CreateObject<Node> ();
    Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
    Ptr<Controller> ctrl = CreateObject<Controller> ();

    of13Helper->InstallController (controllerNode, ctrl);
    of13Helper->InstallSwitch (switchesLow.Get (0), switchPortsLow [0]);
    of13Helper->InstallSwitch (switchesLow.Get (1), switchPortsLow [1]);
    of13Helper->InstallSwitch (switchesLow.Get (2), switchPortsLow [2]);
    of13Helper->InstallSwitch (switchesLow.Get (3), switchPortsLow [3]);
    of13Helper->InstallSwitch (switchesUp.Get (0), switchPortsUp [0]);
    of13Helper->InstallSwitch (switchesUp.Get (1), switchPortsUp [1]);
    of13Helper->InstallSwitch (switchesUp.Get (2), switchPortsUp [2]);
    of13Helper->InstallSwitch (switchesUp.Get (3), switchPortsUp [3]);
    of13Helper->CreateOpenFlowChannels ();



    // TapBridge the controller device to local machine
    // The default configuration expects a controller on local port 6653
    // Install the TCP/IP stack into hosts nodes
    InternetStackHelper internet;
    internet.Install (hosts);

    // Set IPv4 host addresses
    Ipv4AddressHelper ipv4helprLow;
    Ipv4InterfaceContainer hostIpIfacesLow;
    ipv4helprLow.SetBase ("10.1.1.0", "255.255.255.0");
    hostIpIfacesLow = ipv4helprLow.Assign (hostDevicesLow);

    Ipv4AddressHelper ipv4helprUp;
    Ipv4InterfaceContainer hostIpIfacesUp;
    ipv4helprUp.SetBase ("10.2.1.0", "255.255.255.0");
    hostIpIfacesUp = ipv4helprUp.Assign (hostDevicesUp);



//    Ptr<Socket> srcSocket = Socket::CreateSocket (PCMid1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    Ptr<Socket> srcSocketLU = Socket::CreateSocket (PCLeft2, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    srcSocketLU->Bind ();
    Ptr<Socket> srcSocketLL = Socket::CreateSocket (PCLeft1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    srcSocketLL->Bind ();
//
    Ptr<Socket> dstSocket1 = Socket::CreateSocket (PCRight1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint16_t dstport1 = 12345;
    Ipv4Address dstaddr1 = "10.1.1.2";
    InetSocketAddress dst1 = InetSocketAddress (dstaddr1, dstport1);
    dstSocket1->Bind (dst1);
    dstSocket1->SetRecvCallback (MakeCallback (&dstSocketRecv));

    Ptr<Socket> dstSocket2 = Socket::CreateSocket (PCRight1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint16_t dstport2 = 123;
    Ipv4Address dstaddr2 = "10.2.1.2";
    InetSocketAddress dst2 = InetSocketAddress (dstaddr2, dstport2);
    dstSocket2->Bind (dst2);
    dstSocket2->SetRecvCallback (MakeCallback (&dstSocketRecv));

    while (time < rec_time*1.5) {
        Simulator::Schedule (Seconds (time),&SendStuff, srcSocketLL, dstaddr1, dstport1, 0);
        Simulator::Schedule (Seconds (time),&SendStuff, srcSocketLU, dstaddr2, dstport2, 1);
//        Simulator::Schedule (Seconds (time),&SendStuff, srcSocketM, dstaddr, dstport);
        time += interval;
    }

    switch (bad_rec) {
        case 0:
            break;
        case 1:
            Simulator::Schedule (Seconds (rec_time), &OpenFlowCommandRule, 4, ctrl);
            break;
        case 2:
            Simulator::Schedule (Seconds (rec_time), &OpenFlowCommandRule, 8, ctrl);
            break;
        case 3:
            Simulator::Schedule (Seconds (rec_time), &OpenFlowCommandRule, 4, ctrl);
            Simulator::Schedule (Seconds (rec_time), &OpenFlowCommandRule, 8, ctrl);
            break;
        default:
            std::cout << "Bad reconf use option!" << std::endl;
    }

    Simulator::Schedule (Seconds (rec_time*2), &EndSimulation);
    Simulator::Run ();

    Simulator::Destroy ();

    return 0;
}
