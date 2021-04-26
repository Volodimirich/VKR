#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
//#include <ns3/ofswitch13-module.h>
#include "ns3/csma-module.h"
#include <set>
#include <string>
#include <vector>
#include "ns3/openflow-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("FifthScriptExample");

int start = clock();

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


void SendStuff (Ptr<Socket> sock, Ipv4Address dstaddr, uint16_t port) {
    std::cout << "Send to " << dstaddr << std::endl;
//    while(1)
    for (int i = 0; i<100; i++) {
        Ptr<Packet> p = create_packet();
        p->AddPaddingAtEnd (100);
        sock->SendTo(p, 0, InetSocketAddress(dstaddr, port));
    }
    return;
}

void BindSock (Ptr<Socket> sock, Ptr<NetDevice> netdev) {
    sock->BindToNetDevice (netdev);
    return;
}


void dstSocketRecv (Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);
//    std::cout << "Send to " << dstaddr << std::endl;
    float time = get_packet_time(*packet);
    std::cout << "Sending time - " <<  time << " Current time - " << clock() << std::endl;
    packet->RemoveAllPacketTags ();
    packet->RemoveAllByteTags ();
    InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
    NS_LOG_INFO ("Destination Received " << packet->GetSize () << " bytes from " << address.GetIpv4 ());
//    SendStuff (socket, Ipv4Address ("10.10.1.2"), address.GetPort ());
}


int main (int argc, char *argv[]) {
    CommandLine cmd (__FILE__);
    cmd.Parse (argc, argv);

    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    //Adding nodes

    Ptr<Node> SLeft = CreateObject<Node> ();
    Ptr<Node> SMid1 = CreateObject<Node> ();
    Ptr<Node> SMid2 = CreateObject<Node> ();
    Ptr<Node> SRight = CreateObject<Node> ();

    Ptr<Node> PCLeft1 = CreateObject<Node> ();
    Ptr<Node> PCLeft2 = CreateObject<Node> ();
    Ptr<Node> PCRight1 = CreateObject<Node> ();
    Ptr<Node> PCRight2 = CreateObject<Node> ();

    NodeContainer SwContainer = NodeContainer(SLeft, SMid1, SMid2, SRight);
    NodeContainer PCContainer = NodeContainer(PCLeft1, PCLeft2, PCRight1, PCRight2);


    InternetStackHelper internet;
    internet.Install (SwContainer);
    internet.Install (PCContainer);

    // Point-to-point links
    NodeContainer SwLeftMid1 = NodeContainer (SLeft, SMid1);
    NodeContainer SwLeftMid2 = NodeContainer (SLeft, SMid2);
    NodeContainer SwRightMid1 = NodeContainer (SRight, SMid1);
    NodeContainer SwRightMid2 = NodeContainer (SRight, SMid2);


    NodeContainer SwPCLeft1 = NodeContainer (PCLeft1, SLeft);
    NodeContainer SwPCLeft2 = NodeContainer (PCLeft2, SLeft);
    NodeContainer SwPCRight1 = NodeContainer (SRight, PCRight1);
    NodeContainer SwPCRight2 = NodeContainer (SRight, PCRight2);

    std::vector <NodeContainer> SwitchLinks = {SwLeftMid1, SwLeftMid2, SwRightMid1, SwRightMid2};
    std::vector <NodeContainer> PcLinks = {SwPCLeft1, SwPCLeft2, SwPCRight1, SwPCRight2};
    std::vector <Ipv4InterfaceContainer> Interfaces = {};
    std::vector <Ipv4Address> HostAddress = {"10.10.1.0", "10.20.1.0", "10.30.1.0", "10.40.1.0"};
    std::vector <Ipv4Address> SwitchAddress = {"10.1.1.0", "10.2.1.0", "10.3.1.0", "10.4.1.0"};

    // We create the channels first without any IP addressing information
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer terminalDevices;
    NetDeviceContainer switchDevices;
    int pos = 0;
    Ipv4AddressHelper ipv4;


    for (auto &link: SwitchLinks) {
        NetDeviceContainer current = p2p.Install (link);
        ipv4.SetBase (SwitchAddress[pos], "255.255.255.0");
        pos += 1;
        ipv4.Assign (current);
        switchDevices.Add(current.Get(0));
        switchDevices.Add(current.Get(1));
    }

    pos = 0;
    for (auto &link: PcLinks) {
        NetDeviceContainer current = p2p.Install(link);
        ipv4.SetBase (HostAddress[pos], "255.255.255.0");
        pos+=1;
        ipv4.Assign (current);
        terminalDevices.Add(current.Get(0));
        switchDevices.Add(current.Get(1));
    }


    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingPCL1 = ipv4RoutingHelper.GetStaticRouting (PCLeft1->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingPCL2 = ipv4RoutingHelper.GetStaticRouting (PCLeft2->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingPCR1 = ipv4RoutingHelper.GetStaticRouting (PCRight1->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingPCR2 = ipv4RoutingHelper.GetStaticRouting (PCRight2->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingSwL = ipv4RoutingHelper.GetStaticRouting (SLeft->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingSwM1 = ipv4RoutingHelper.GetStaticRouting (SMid1->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingSwM2 = ipv4RoutingHelper.GetStaticRouting (SMid2->GetObject<Ipv4> ());
    Ptr<Ipv4StaticRouting> staticRoutingSwR = ipv4RoutingHelper.GetStaticRouting (SRight->GetObject<Ipv4> ());

    // Create static routes from Src to Dst

    staticRoutingPCL1->AddHostRouteTo(Ipv4Address("10.40.1.2"), Ipv4Address("10.1.1.2"), 1, 5);

    staticRoutingSwL->AddHostRouteTo (Ipv4Address ("10.40.1.2"), Ipv4Address ("10.2.1.2"), 1,5);
    staticRoutingSwL->AddHostRouteTo (Ipv4Address ("10.40.1.2"), Ipv4Address ("10.3.1.2"), 2,10);


    staticRoutingSwM1->AddHostRouteTo (Ipv4Address ("10.40.1.2"), Ipv4Address ("10.4.1.2"), 2,10);
    staticRoutingSwM1->AddHostRouteTo (Ipv4Address ("10.10.1.1"), Ipv4Address ("10.1.1.1"), 2,10);
//
    staticRoutingSwM2->AddHostRouteTo (Ipv4Address ("10.40.1.2"), Ipv4Address ("10.4.1.2"), 1,10);
    staticRoutingSwM2->AddHostRouteTo (Ipv4Address ("10.10.1.1"), Ipv4Address ("10.1.1.1"), 1,10);

    staticRoutingSwR->AddHostRouteTo (Ipv4Address ("10.10.1.1"), Ipv4Address ("10.2.1.1"), 1,5);
    staticRoutingSwR->AddHostRouteTo (Ipv4Address ("10.10.1.1"), Ipv4Address ("10.3.1.1"), 2,10);

    staticRoutingPCR2->AddHostRouteTo (Ipv4Address ("10.10.1.1"), Ipv4Address ("10.4.1.2"), 1,5);
//
    // There are no apps that can utilize the Socket Option so doing the work directly..
    // Taken from tcp-large-transfer example


    Ptr<Socket> srcSocket = Socket::CreateSocket (PCLeft1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    srcSocket->Bind ();

    Ptr<Socket> dstSocket = Socket::CreateSocket (PCRight2, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint16_t dstport = 12345;
    Ipv4Address dstaddr ("10.40.1.2");
    InetSocketAddress dst = InetSocketAddress (dstaddr, dstport);
    dstSocket->Bind (dst);
    dstSocket->SetRecvCallback (MakeCallback (&dstSocketRecv));

    OpenFlowSwitchHelper swtch;
    swtch.AddFlow();

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll (ascii.CreateFileStream ("socket-bound-static-routing.tr"));
    p2p.EnablePcapAll ("socket-bound-static-routing");

    std::cout << "start!" << std::endl;
    Simulator::Schedule (Seconds (0.1),&SendStuff, srcSocket, dstaddr, dstport);
    Simulator::Run ();
    Simulator::Destroy ();

    //file.close();
    return 0;
}
