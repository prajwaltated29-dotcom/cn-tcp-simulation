#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredTcpComparison");

std::string tcpVariant = "TcpLinuxReno";
Ptr<OutputStreamWrapper> cwndStream;

static void CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
{
    *cwndStream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << "\n";
}

static void ConnectCwndTrace(uint32_t nodeId, uint32_t socketId)
{
    std::string path = "/NodeList/" + std::to_string(nodeId) +
                       "/$ns3::TcpL4Protocol/SocketList/" +
                       std::to_string(socketId) + "/CongestionWindow";
    Config::ConnectWithoutContext(path, MakeCallback(&CwndChange));
}

int main(int argc, char* argv[])
{
    double      simTime        = 100.0;
    std::string bottleneckBw   = "8Mbps";
    std::string bottleneckDelay= "50ms";
    double      lossRate       = 0.01;
    uint32_t    nSenders       = 2;
    uint32_t    nRouters       = 2;   // chain: R0 -> R1 -> ... -> R(n-1)

    CommandLine cmd;
    cmd.AddValue("tcpVariant",       "TCP variant",               tcpVariant);
    cmd.AddValue("lossRate",         "Packet loss rate",          lossRate);
    cmd.AddValue("bottleneckBw",     "Bottleneck bandwidth",      bottleneckBw);
    cmd.AddValue("bottleneckDelay",  "Per-hop delay",             bottleneckDelay);
    cmd.AddValue("nSenders",         "Number of sender pairs",    nSenders);
    cmd.AddValue("nRouters",         "Number of routers (2-8)",   nRouters);
    cmd.AddValue("simTime",          "Simulation time (s)",       simTime);
    cmd.Parse(argc, argv);

    if (nSenders < 1)  nSenders = 1;
    if (nSenders > 20) nSenders = 20;
    if (nRouters < 2)  nRouters = 2;
    if (nRouters > 8)  nRouters = 8;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::" + tcpVariant));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

    // ── Nodes ─────────────────────────────────────────────────────────────────
    NodeContainer senders,  receivers;
    senders.Create(nSenders);
    receivers.Create(nSenders);

    // Router chain: routers[0] --- routers[1] --- ... --- routers[nRouters-1]
    NodeContainer routers;
    routers.Create(nRouters);

    // ── Links ─────────────────────────────────────────────────────────────────
    PointToPointHelper accessLink, routerLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay",   StringValue("5ms"));

    // Each router-to-router link uses the configured bottleneck bandwidth/delay
    routerLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBw));
    routerLink.SetChannelAttribute("Delay",   StringValue(bottleneckDelay));

    // ── Connect senders to first router ──────────────────────────────────────
    std::vector<NetDeviceContainer> senderDevs(nSenders);
    for (uint32_t i = 0; i < nSenders; i++)
        senderDevs[i] = accessLink.Install(senders.Get(i), routers.Get(0));

    // ── Chain routers together (each hop = bottleneck link) ───────────────────
    // Apply loss model only on the FIRST router-to-router link (bottleneck)
    std::vector<NetDeviceContainer> routerLinks(nRouters - 1);
    for (uint32_t i = 0; i < nRouters - 1; i++) {
        routerLinks[i] = routerLink.Install(routers.Get(i), routers.Get(i + 1));
        if (i == 0) {
            // Loss on first inter-router link only
            Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
            em->SetAttribute("ErrorRate", DoubleValue(lossRate));
            em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
            routerLinks[i].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
        }
    }

    // ── Connect last router to receivers ─────────────────────────────────────
    std::vector<NetDeviceContainer> receiverDevs(nSenders);
    for (uint32_t i = 0; i < nSenders; i++)
        receiverDevs[i] = accessLink.Install(routers.Get(nRouters - 1), receivers.Get(i));

    // ── Internet stack ────────────────────────────────────────────────────────
    InternetStackHelper stack;
    stack.InstallAll();

    // ── IP addressing ─────────────────────────────────────────────────────────
    Ipv4AddressHelper ipv4;

    // Sender subnets: 10.1.i.0
    for (uint32_t i = 0; i < nSenders; i++) {
        std::string base = "10.1." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(base.c_str(), "255.255.255.0");
        ipv4.Assign(senderDevs[i]);
    }

    // Router-to-router subnets: 10.2.i.0
    for (uint32_t i = 0; i < nRouters - 1; i++) {
        std::string base = "10.2." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(base.c_str(), "255.255.255.0");
        ipv4.Assign(routerLinks[i]);
    }

    // Receiver subnets: 10.3.i.0
    std::vector<Ipv4InterfaceContainer> recvIfaces(nSenders);
    for (uint32_t i = 0; i < nSenders; i++) {
        std::string base = "10.3." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(base.c_str(), "255.255.255.0");
        recvIfaces[i] = ipv4.Assign(receiverDevs[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ── Applications ──────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < nSenders; i++) {
        uint16_t port = 9 + i;

        PacketSinkHelper sink("ns3::TcpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), port));
        sink.Install(receivers.Get(i)).Start(Seconds(0.0));

        BulkSendHelper bulk("ns3::TcpSocketFactory",
            InetSocketAddress(recvIfaces[i].GetAddress(1), port));
        bulk.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer app = bulk.Install(senders.Get(i));
        app.Start(Seconds(0.1 + i * 0.4));
        app.Stop(Seconds(simTime));
    }

    // ── CWND trace ────────────────────────────────────────────────────────────
    AsciiTraceHelper ascii;
    cwndStream = ascii.CreateFileStream(
        "tcp-wired-" + tcpVariant + "-cwnd.dat");
    Simulator::Schedule(Seconds(0.2), &ConnectCwndTrace, 0, 0);

    // ── Flow monitor ──────────────────────────────────────────────────────────
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->SerializeToXmlFile(
        "tcp-wired-" + tcpVariant + "-flowmon.xml", true, true);
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    auto stats = monitor->GetFlowStats();

    double sumTh = 0, sumThSq = 0;
    int n = stats.size();

    std::cout << "\n=== Results for " << tcpVariant << " (Wired, "
              << nRouters << " routers) ===\n";
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