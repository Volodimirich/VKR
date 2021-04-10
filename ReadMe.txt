This script exercises global routing code in a mixed point-to-point
 and csma/cd environment.  We bring up and down interfaces and observe
 the effect on global routing.  We explicitly enable the attribute
 to respond to interface events, so that routes are recomputed
 automatically.

 Network topology

  n0
     \ p-p
      \          (shared csma/cd)
       n2 -------------------------n3
      /            |        |
     / p-p        n4        n5 ---------- n6
   n1                             p-p
   |                                      |
   ----------------------------------------
                p-p

 - at time 1 CBR/UDP flow from n1 to n6's IP address on the n5/n6 link
 - at time 10, start similar flow from n1 to n6's address on the n1/n6 link

  Order of events
  At pre-simulation time, configure global routes.  Shortest path from
  n1 to n6 is via the direct point-to-point link
  At time 1s, start CBR traffic flow from n1 to n6
  At time 2s, set the n1 point-to-point interface to down.  Packets
    will be diverted to the n1-n2-n5-n6 path
  At time 4s, re-enable the n1/n6 interface to up.  n1-n6 route restored.
  At time 6s, set the n6-n1 point-to-point Ipv4 interface to down (note, this
    keeps the point-to-point link "up" from n1's perspective).  Traffic will
    flow through the path n1-n2-n5-n6
  At time 8s, bring the interface back up.  Path n1-n6 is restored
  At time 10s, stop the first flow.
  At time 11s, start a new flow, but to n6's other IP address (the one
    on the n1/n6 p2p link)
  At time 12s, bring the n1 interface down between n1 and n6.  Packets
    will be diverted to the alternate path
  At time 14s, re-enable the n1/n6 interface to up.  This will change
    routing back to n1-n6 since the interface up notification will cause
    a new local interface route, at higher priority than global routing
  At time 16s, stop the second flow.

 - Tracing of queues and packet receptions to file "dynamic-global-routing.tr"


Available options:

PointToPointDevice:
    Mtu: The MAC-level Maximum Transmission Unit
    Address: The MAC address of this device.
    DataRate: The default data rate for point to point links
    ReceiveErrorModel: The receiver error model used to simulate packet loss
    InterframeGap: The time to wait between packet (frame) transmissions
    TxQueue: A queue to use as the transmit queue in the device.

PointToPointChannel
    Delay: Transmission delay through the channel


CsmaNetDevice
    Address: The MAC address of this device.
    Mtu: The MAC-level Maximum Transmission Unit
    EncapsulationMode: The link-layer encapsulation type to use.
    SendEnable: Enable or disable the transmitter section of the device.
    ReceiveEnable: Enable or disable the receiver section of the device.
    ReceiveErrorModel: The receiver error model used to simulate packet loss
    TxQueue: A queue to use as the transmit queue in the device.

CsmaChannel
    DataRate: The transmission data rate to be provided to devices connected to the channel
    Delay: Transmission delay through the channel

cp diplom/main.cc scratch/diplom.cc
./waf --run scratch/diplom.cc 


