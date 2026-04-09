#ifndef SUBMISSION_WIRED_H
#define SUBMISSION_WIRED_H

#include "submission-common.h"

#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/network-module.h"

namespace submissioncase4
{
using namespace ns3;

inline void
InstallTcpFlowsIpv4(const NodeContainer& nodes,
                    const Ipv4InterfaceContainer& interfaces,
                    uint32_t packetSize,
                    uint32_t flowCount,
                    uint32_t packetsPerSecond,
                    double startTime,
                    double stopTime)
{
    const auto flows = BuildFlows(nodes.GetN(), flowCount);
    uint16_t port = 9000;
    const std::string rate = std::to_string(packetSize * packetsPerSecond * 8) + "bps";

    for (const auto& flow : flows)
    {
        PacketSinkHelper sinkHelper(
            "ns3::TcpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(flow.second));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(stopTime));

        OnOffHelper clientHelper(
            "ns3::TcpSocketFactory",
            InetSocketAddress(interfaces.GetAddress(flow.second), port));
        clientHelper.SetAttribute("OnTime",
                                  StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        clientHelper.SetAttribute("OffTime",
                                  StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientHelper.SetAttribute("DataRate", DataRateValue(DataRate(rate)));

        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(flow.first));
        clientApp.Start(Seconds(startTime + (port - 9000) * 0.2));
        clientApp.Stop(Seconds(stopTime));
        ++port;
    }
}

inline Metrics
CollectIpv4Metrics(Ptr<FlowMonitor> monitor, double activeSeconds)
{
    Metrics metrics;
    uint64_t totalRxBytes = 0;
    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalLostPackets = 0;
    double totalDelaySeconds = 0.0;

    for (const auto& flow : monitor->GetFlowStats())
    {
        const FlowMonitor::FlowStats& st = flow.second;
        totalRxBytes += st.rxBytes;
        totalTxPackets += st.txPackets;
        totalRxPackets += st.rxPackets;
        totalLostPackets += st.lostPackets;
        totalDelaySeconds += st.delaySum.GetSeconds();
    }

    metrics.throughputKbps = (totalRxBytes * 8.0) / (activeSeconds * 1000.0);
    metrics.averageDelayMs =
        totalRxPackets > 0 ? (totalDelaySeconds * 1000.0) / totalRxPackets : 0.0;
    metrics.deliveryRatio =
        totalTxPackets > 0 ? static_cast<double>(totalRxPackets) / totalTxPackets : 0.0;
    metrics.dropRatio =
        totalTxPackets > 0 ? static_cast<double>(totalLostPackets) / totalTxPackets : 0.0;
    return metrics;
}

inline Metrics
RunWiredSimulation(uint32_t nodes,
                   uint32_t flowCount,
                   uint32_t packetsPerSecond,
                   uint32_t packetSize,
                   double simTime,
                   bool useQrtt,
                   bool useFuzzyQrtt)
{
    ConfigureQrtt(useQrtt, useFuzzyQrtt);

    NodeContainer wiredNodes;
    wiredNodes.Create(nodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(50)));
    NetDeviceContainer devices = csma.Install(wiredNodes);

    InternetStackHelper internet;
    internet.Install(wiredNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    InstallTcpFlowsIpv4(
        wiredNodes, interfaces, packetSize, flowCount, packetsPerSecond, 2.0, simTime);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    monitor->CheckForLostPackets();

    Metrics metrics = CollectIpv4Metrics(monitor, simTime - 2.0);
    Simulator::Destroy();
    return metrics;
}

} // namespace submissioncase4

#endif
