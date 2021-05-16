#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>
#include "ns3/applications-module.h"
#include <unistd.h>

enum event {NorthSendPacket, SouthSendPacket, Reconfiguration,  Waiting};
enum experiment_type {None, Balanced, Stopped};

using namespace ns3;
using scheduler = std::vector<std::pair<float, event>>;

namespace {
    CsmaHelper csmaHelper;
    CsmaHelper csmaStopped;
    std::ofstream out;
    int counter1 = 0;
    int counter2 = 0;
}

class Controller : public OFSwitch13Controller
{
public:
    void BadReconf (uint64_t swtch) {
        DpctlExecute (swtch, "flow-mod cmd=del,table=0,prio=1 in_port=2 write:output=1");
    }
protected:
    void HandshakeSuccessful (Ptr<const RemoteSwitch> swtch) {
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=2 in_port=1 write:output=2");
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=2 in_port=2 write:output=1");
    }
};


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

    int time_now = Now().GetMicroSeconds();
    data_int[0] = time_now;
    data_int[1] = ct;
    data_int[2] = from;

    return Create<Packet> (data, size);
}

void dst_socket_recv (Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);
    int snd, counter, send_from;
    std::tie(snd, counter, send_from) = get_packet_time(*packet);

    int rsv = Now().GetMicroSeconds();
    std::cout << "Sending time - <" <<  ((double) snd /1000000) << "> Current time - <" << ((double) rsv/1000000) << "> Packet name - <" << counter << "> Sended from - <" << send_from << ">" << std::endl;
    out << "Sending time - <" <<  ((double) snd /1000000) << "> Current time - <" << ((double) rsv/1000000) << "> Packet name - <" << counter << "> Sended from - <" << send_from << ">" << std::endl;
    packet->RemoveAllPacketTags ();
    packet->RemoveAllByteTags ();
}


void send_stuff (Ptr<Socket> sock, Ipv4Address dstaddr, uint16_t port, int from) {
    Ptr <Packet> p = create_packet(from);
    p->AddPaddingAtEnd(100);
    std::cout << "Send to " << dstaddr << std::endl;
    sock->SendTo(p, 0, InetSocketAddress(dstaddr, port));
    return;
}

void EndSimulation() {
    Simulator::Stop();
}

void switch_install(std::pair<size_t, size_t> indexes, NetDeviceContainer* switchPorts, NodeContainer switches) {
    NodeContainer pair = NodeContainer (switches.Get (indexes.first), switches.Get (indexes.second));
    NetDeviceContainer pairDevs = csmaHelper.Install (pair);
    switchPorts [indexes.first].Add (pairDevs.Get (0));
    switchPorts [indexes.second].Add (pairDevs.Get (1));
}

void AddDelay(CsmaHelper csma, DataRate rate) {
    csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (rate)));

}

void OpenFlowCommandRule(uint64_t dpid,  Ptr<Controller> ctrl ) {
    ctrl->BadReconf(dpid);
}

void Nothing() {
    return;
}

scheduler CreateSchedule(std::tuple<float, float, float>  time_events, experiment_type EventType) {
    float next_nth_snd, next_sth_snd, rec_time;
    bool event_completed, delivered_north, delivered_south;
    std::tie(next_nth_snd, next_sth_snd, rec_time) = time_events;
    float nth_interval, sth_interval;
    float cur_time = 0;
    float general_time = std::min(next_sth_snd, next_nth_snd);
    float sch_delay = general_time/10;
    scheduler time_schedule;

    std::tie(next_nth_snd, next_sth_snd, rec_time) = time_events;
    nth_interval = next_nth_snd;
    sth_interval = next_sth_snd;
    delivered_north = delivered_south = false;

    while (cur_time < rec_time*2) {
        event_completed = false;
        if (cur_time >= next_nth_snd) {
            time_schedule.emplace_back(std::make_pair(cur_time, NorthSendPacket));
            next_nth_snd+= nth_interval;
            event_completed = true;
            if (EventType == Balanced) {
                if (delivered_north && delivered_south) {
                    next_nth_snd = next_sth_snd;
                    nth_interval = sth_interval;
                } else {
                    delivered_north = true;
                }
            }
        }
        if (cur_time >= next_sth_snd) {
            time_schedule.emplace_back(std::make_pair(cur_time, SouthSendPacket));
            next_sth_snd+= sth_interval;
            event_completed = true;
            if (EventType == Balanced) {
                if (delivered_north && delivered_south ) {
                    next_sth_snd =  next_nth_snd;
                    sth_interval = nth_interval;
                } else {
                    delivered_south = true;
                }
            }
        }
        if (cur_time >= rec_time) {
            time_schedule.emplace_back(std::make_pair(cur_time, Reconfiguration));
            event_completed = true;
        }

        if (!event_completed) {
            time_schedule.emplace_back(std::make_pair(cur_time, Waiting));
        }

        cur_time+=sch_delay;
    }
    return time_schedule;
}

int main (int argc, char *argv[])
{
    int bad_rec, last;
    float interval_up = 0.1;
    float interval_dwn = 0.1;
    float rec_time = 10;
    int delay = 2;
    int exp_type = 0;
    experiment_type exp_val = None;
    std::string daterate = "100Mbps";

    CommandLine cmd;
    out.open("data.txt"); // окрываем файл для записи

    cmd.AddValue ("daterate", "Csma daterate", daterate);
    cmd.AddValue ("delay", "Csma delay(MilliSeconds)", delay);
    cmd.AddValue ("time_between_packages_up", "Time between sending packets", interval_up);
    cmd.AddValue ("time_between_packages_dwn", "Time between sending packets", interval_dwn);
    cmd.AddValue ("reconfiguration", "Reconfiguration time", rec_time);
    cmd.AddValue ("bad_reconf_on", "Where in bad reconfiguration? 3-everythere, 2-Upper, 1-Lower, 0-Nowhere", bad_rec);
    cmd.AddValue ("experiment", "Choose type of experiment? 2-Stopped, 1-Balanced, 0-None", exp_type);

    if (exp_type>= 0 and exp_type<=2)
        exp_val = static_cast<experiment_type>(exp_type);
    cmd.Parse (argc, argv);

    out << "Csma daterate - " <<  daterate << std::endl;
    out << "Csma delay - " <<  delay << std::endl;
    out << "Time between sending north packets - " <<  interval_up << std::endl;
    out << "Time between sending south packets - " <<  interval_dwn << std::endl;
    out << "Reconfiguration time - " <<  rec_time << std::endl;

    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    Ptr<Node> PCLeft1 = CreateObject<Node> ();
    Ptr<Node> PCLeft2 = CreateObject<Node> ();

    Ptr<Node> PCMid1 = CreateObject<Node> ();
    Ptr<Node> PCMid2 = CreateObject<Node> ();

    Ptr<Node> PCRight1 = CreateObject<Node> ();
    NodeContainer hostsLow = NodeContainer(PCLeft1, PCMid1, PCRight1);
    NodeContainer hostsUp = NodeContainer(PCLeft2, PCMid2, PCRight1);
    NodeContainer hosts = NodeContainer(PCLeft1,  PCLeft2, PCMid1, PCMid2, PCRight1);

    NodeContainer switchesLow;
    NodeContainer switchesUp;

    switchesLow.Create (4);
    switchesUp.Create (4);

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

    csmaStopped.SetChannelAttribute ("DataRate", DataRateValue (DataRate (daterate)));
    csmaStopped.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));

    pair = NodeContainer(hostsLow.Get(2), switchesLow.Get(3));
    pairDevs = csmaStopped.Install(pair);
    hostDevicesLow.Add(pairDevs.Get(0));
    switchPortsLow[3].Add(pairDevs.Get(1));

    pair = NodeContainer(hostsUp.Get(2), switchesUp.Get(3));

    pairDevs = csmaHelper.Install(pair);
    hostDevicesUp.Add(pairDevs.Get(0));
    switchPortsUp[3].Add(pairDevs.Get(1));

    switch_install(std::make_pair(0, 1), switchPortsLow, switchesLow);
    switch_install(std::make_pair(1, 2), switchPortsLow, switchesLow);
    switch_install(std::make_pair(2, 3), switchPortsLow, switchesLow);

    switch_install(std::make_pair(0, 1), switchPortsUp, switchesUp);
    switch_install(std::make_pair(1, 2), switchPortsUp, switchesUp);
    switch_install(std::make_pair(2, 3), switchPortsUp, switchesUp);

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

    InternetStackHelper internet;
    internet.Install (hosts);

    Ipv4AddressHelper ipv4helprLow;
    Ipv4InterfaceContainer hostIpIfacesLow;
    ipv4helprLow.SetBase ("10.1.1.0", "255.255.255.0");
    hostIpIfacesLow = ipv4helprLow.Assign (hostDevicesLow);

    Ipv4AddressHelper ipv4helprUp;
    Ipv4InterfaceContainer hostIpIfacesUp;
    ipv4helprUp.SetBase ("10.2.1.0", "255.255.255.0");
    hostIpIfacesUp = ipv4helprUp.Assign (hostDevicesUp);


    Ptr<Socket> srcSocketLU = Socket::CreateSocket (PCLeft2, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    srcSocketLU->Bind ();
    Ptr<Socket> srcSocketLL = Socket::CreateSocket (PCLeft1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    srcSocketLL->Bind ();

    Ptr<Socket> dstSocket1 = Socket::CreateSocket (PCRight1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint16_t dstport1 = 12345;
    Ipv4Address dstaddr1 = "10.1.1.2";
    InetSocketAddress dst1 = InetSocketAddress (dstaddr1, dstport1);
    dstSocket1->Bind (dst1);
    dstSocket1->SetRecvCallback (MakeCallback (&dst_socket_recv));

    Ptr<Socket> dstSocket2 = Socket::CreateSocket (PCRight1, TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint16_t dstport2 = 123;
    Ipv4Address dstaddr2 = "10.2.1.2";
    InetSocketAddress dst2 = InetSocketAddress (dstaddr2, dstport2);
    dstSocket2->Bind (dst2);
    dstSocket2->SetRecvCallback (MakeCallback (&dst_socket_recv));

    scheduler time_schedule = CreateSchedule(std::make_tuple(interval_up, interval_dwn, rec_time), exp_val);

    for (auto &element : time_schedule) {
        switch (element.second) {
            case NorthSendPacket:
                Simulator::Schedule(Seconds (element.first) , &send_stuff, srcSocketLL, dstaddr1, dstport1, 0);
                if (exp_val == Stopped) {
                    Simulator::Schedule(Seconds(element.first), &AddDelay, csmaStopped, DataRate ("0Mbps"));
                }

                break;
            case SouthSendPacket:
                Simulator::Schedule(Seconds (element.first) , &send_stuff, srcSocketLU, dstaddr2, dstport2, 1);
                break;
            case Reconfiguration:
                switch (bad_rec) {
                    case 0:
                        break;
                    case 1:
                        Simulator::Schedule(Seconds(element.first), &OpenFlowCommandRule, 4, ctrl);
                        break;
                    case 2:
                        Simulator::Schedule(Seconds(element.first), &OpenFlowCommandRule, 8, ctrl);
                        break;
                    case 3:
                        Simulator::Schedule(Seconds(element.first), &OpenFlowCommandRule, 4, ctrl);
                        Simulator::Schedule(Seconds(element.first), &OpenFlowCommandRule, 8, ctrl);
                        break;
                    default:
                        exit(0);
                }
                break;
            case Waiting:
                Simulator::Schedule(Seconds(element.first), &Nothing);
                break;
        }
        last = element.first;
    }

    Simulator::Schedule(Seconds(last), &AddDelay, csmaStopped, DataRate ("100Mbps"));

    Simulator::Stop (Seconds (11.0));
    Simulator::Run ();

    Simulator::Destroy ();

    return 0;
}