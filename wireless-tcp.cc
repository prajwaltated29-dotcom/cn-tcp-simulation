#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessTcpComparison");

std::string tcpVariant = "TcpLinuxReno";
Ptr<OutputStreamWrapper> cwndStream;
uint32_t globalCwnd = 0;

static void CwndChange(uint32_t oldCwnd, uint32_t newCwnd) {
    globalCwnd = newCwnd;
}

static void WriteCwndPeriodic() {
    if (cwndStream && globalCwnd > 0) {
        *cwndStream->GetStream() << Simulator::Now().GetSeconds() << "\t" << globalCwnd << std::endl;
    }
    Simulator::Schedule(Seconds(0.1), &WriteCwndPeriodic);
}

static void ConnectCwndTrace(uint32_t nodeId, uint32_t socketId) {
    std::string path = "/NodeList/" + std::to_string(nodeId) +
                       "/$ns3::TcpL4Protocol/SocketList/" +
                       std::to_string(socketId) + "/CongestionWindow";
    Config::ConnectWithoutContext(path, MakeCallback(&CwndChange));
    Simulator::ScheduleNow(&WriteCwndPeriodic);
}

int main(int argc, char* argv[])
{
    double      simTime        = 100.0;
    std::string bottleneckBw   = "8Mbps";
    std::string bottleneckDelay= "50ms";
    double      lossRate       = 0.01;
    uint32_t    nSenders       = 2;
    uint32_t    nReceivers     = 2;
    uint32_t    nRouters       = 2;

    CommandLine cmd;
    cmd.AddValue("tcpVariant",       "TCP variant",               tcpVariant);
    cmd.AddValue("lossRate",         "Packet loss rate",          lossRate);
    cmd.AddValue("bottleneckBw",     "Bottleneck bandwidth",      bottleneckBw);
    cmd.AddValue("bottleneckDelay",  "Per-hop delay",             bottleneckDelay);
    cmd.AddValue("nSenders",         "Number of senders",         nSenders);
    cmd.AddValue("nReceivers",       "Number of receivers",       nReceivers);
    cmd.AddValue("nRouters",         "Number of routers",         nRouters);
    cmd.AddValue("simTime",          "Simulation time (s)",       simTime);
    cmd.Parse(argc, argv);

    if (nSenders < 1)  nSenders = 1;
    if (nReceivers < 1) nReceivers = 1;
    if (nRouters < 2)  nRouters = 2;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpVariant));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

    NodeContainer wifiSenders;    wifiSenders.Create(nSenders);
    NodeContainer routers;        routers.Create(nRouters); // AP is routers.Get(0)
    NodeContainer wiredReceivers; wiredReceivers.Create(nReceivers);

    // ── WiFi setup (802.11n) for Senders -> First Router ──
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("tcp-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiSenders);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, routers.Get(0));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nSenders), "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiSenders);
    
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(2.5), "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1), "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(routers.Get(0));

    // ── Backbone Router Links (with Bottleneck & Delay) ──
    PointToPointHelper routerLink;
    routerLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBw));
    routerLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    std::vector<NetDeviceContainer> routerLinks(nRouters - 1);
    for (uint32_t i = 0; i < nRouters - 1; i++) {
        routerLinks[i] = routerLink.Install(routers.Get(i), routers.Get(i + 1));
        if (i == 0 && lossRate > 0) {
            Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
            em->SetAttribute("ErrorRate", DoubleValue(lossRate));
            em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
            routerLinks[i].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
        }
    }

    // ── Access links to receivers ──
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("5ms"));

    std::vector<NetDeviceContainer> receiverDevs(nReceivers);
    for (uint32_t i = 0; i < nReceivers; i++) {
        receiverDevs[i] = accessLink.Install(routers.Get(nRouters - 1), wiredReceivers.Get(i));
    }

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiSenders);
    stack.Install(routers);
    stack.Install(wiredReceivers);

    // IP Addressing
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(staDevices);
    ipv4.Assign(apDevice);

    for (uint32_t i = 0; i < nRouters - 1; i++) {
        std::string base = "10.2." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(base.c_str(), "255.255.255.0");
        ipv4.Assign(routerLinks[i]);
    }

    std::vector<Ipv4InterfaceContainer> recvIfaces(nReceivers);
    for (uint32_t i = 0; i < nReceivers; i++) {
        std::string base = "10.3." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(base.c_str(), "255.255.255.0");
        recvIfaces[i] = ipv4.Assign(receiverDevs[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    for (uint32_t i = 0; i < nReceivers; i++) {
        uint16_t port = 9 + i;
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sink.Install(wiredReceivers.Get(i)).Start(Seconds(0.0));
    }

    for (uint32_t i = 0; i < nSenders; i++) {
        uint32_t targetRcv = i % nReceivers;
        uint16_t port = 9 + targetRcv;
        BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(recvIfaces[targetRcv].GetAddress(1), port));
        bulk.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer app = bulk.Install(wifiSenders.Get(i));
        app.Start(Seconds(1.0 + i * 0.5));
        app.Stop(Seconds(simTime));
    }

    // Trace
    AsciiTraceHelper ascii;
    cwndStream = ascii.CreateFileStream("tcp-wireless-" + tcpVariant + "-cwnd.dat");
    Simulator::Schedule(Seconds(1.1), &ConnectCwndTrace, 0, 0);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->SerializeToXmlFile("tcp-wireless-" + tcpVariant + "-flowmon.xml", true, true);
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    auto stats = monitor->GetFlowStats();

    double sumTh = 0, sumThSq = 0;
    int n = stats.size();

    std::cout << "\n=== Results for " << tcpVariant << " (Wireless) ===\n";
    for (auto& flow : stats) {
        auto t = classifier->FindFlow(flow.first);
        double th    = flow.second.rxBytes * 8.0 / simTime / 1e6;
        double delay = (flow.second.rxPackets > 0)
            ? flow.second.delaySum.GetSeconds() / flow.second.rxPackets * 1000 : 0;
        double loss  = (flow.second.txPackets > 0)
            ? (double)flow.second.lostPackets / flow.second.txPackets * 100 : 0;
        sumTh   += th;
        sumThSq += th * th;
        std::cout << "Flow " << flow.first
                  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n"
                  << "  Throughput : " << th    << " Mbps\n"
                  << "  Avg Delay  : " << delay << " ms\n"
                  << "  Loss Ratio : " << loss  << " %\n";
    }
    double jfi = (n > 0 && sumThSq > 0) ? (sumTh * sumTh) / (n * sumThSq) : 0;
    std::cout << "  Total Throughput : " << sumTh << " Mbps\n";
    std::cout << "  Jain's Fairness  : " << jfi  << "\n\n";

    Simulator::Destroy();
    return 0;
}