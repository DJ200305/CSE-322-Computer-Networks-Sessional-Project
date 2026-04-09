#ifndef SUBMISSION_LRWPAN_MOBILE_H
#define SUBMISSION_LRWPAN_MOBILE_H

#include "submission-common.h"

#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/lr-wpan-mac.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/lr-wpan-phy.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ripng-helper.h"
#include "ns3/sixlowpan-module.h"

#include <cstdio>
#include <unordered_map>

namespace submissioncase4
{
using namespace ns3;

inline std::unordered_map<uint64_t, Ptr<energy::SimpleDeviceEnergyModel>>&
GetEnergyModels()
{
    static std::unordered_map<uint64_t, Ptr<energy::SimpleDeviceEnergyModel>> energyModels;
    return energyModels;
}

inline uint64_t
MakeEnergyKey(uint32_t nodeId, uint32_t devId)
{
    return (static_cast<uint64_t>(nodeId) << 32) | devId;
}

inline void
UpdateLrWpanCurrent(std::string context,
                    lrwpan::PhyEnumeration oldState,
                    lrwpan::PhyEnumeration newState)
{
    (void)oldState;
    uint32_t nodeId = 0;
    uint32_t devId = 0;
    if (std::sscanf(context.c_str(), "/NodeList/%u/DeviceList/%u", &nodeId, &devId) != 2)
    {
        return;
    }

    auto& energyModels = GetEnergyModels();
    auto it = energyModels.find(MakeEnergyKey(nodeId, devId));
    if (it == energyModels.end())
    {
        return;
    }

    constexpr double kTxCurrentA = 0.0174;
    constexpr double kRxCurrentA = 0.0188;
    constexpr double kIdleCurrentA = 0.000426;
    constexpr double kSleepCurrentA = 0.00002;

    double currentA = kIdleCurrentA;
    switch (newState)
    {
    case lrwpan::IEEE_802_15_4_PHY_BUSY_TX:
        currentA = kTxCurrentA;
        break;
    case lrwpan::IEEE_802_15_4_PHY_BUSY_RX:
        currentA = kRxCurrentA;
        break;
    case lrwpan::IEEE_802_15_4_PHY_TRX_OFF:
        currentA = kSleepCurrentA;
        break;
    default:
        break;
    }
    it->second->SetCurrentA(currentA);
}

inline void
InstallTcpFlowsIpv6(const NodeContainer& nodes,
                    const Ipv6InterfaceContainer& interfaces,
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
            Inet6SocketAddress(Ipv6Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(flow.second));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(stopTime));

        OnOffHelper clientHelper(
            "ns3::TcpSocketFactory",
            Inet6SocketAddress(interfaces.GetAddress(flow.second, 1), port));
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
RunLrWpanMobileSimulation(uint32_t nodes,
                          uint32_t flowCount,
                          uint32_t packetsPerSecond,
                          uint32_t packetSize,
                          double speed,
                          double simTime,
                          bool useQrtt,
                          bool useFuzzyQrtt)
{
    ConfigureQrtt(useQrtt, useFuzzyQrtt);
    auto& energyModels = GetEnergyModels();
    energyModels.clear();

    NodeContainer mobileNodes;
    mobileNodes.Create(nodes);

    Ptr<UniformRandomVariable> speedRv = CreateObject<UniformRandomVariable>();
    speedRv->SetAttribute("Min", DoubleValue(speed));
    speedRv->SetAttribute("Max", DoubleValue(speed));

    Ptr<UniformRandomVariable> pauseRv = CreateObject<UniformRandomVariable>();
    pauseRv->SetAttribute("Min", DoubleValue(0.2));
    pauseRv->SetAttribute("Max", DoubleValue(0.8));

    Ptr<UniformRandomVariable> xRv = CreateObject<UniformRandomVariable>();
    xRv->SetAttribute("Min", DoubleValue(0.0));
    xRv->SetAttribute("Max", DoubleValue(200.0));

    Ptr<UniformRandomVariable> yRv = CreateObject<UniformRandomVariable>();
    yRv->SetAttribute("Min", DoubleValue(0.0));
    yRv->SetAttribute("Max", DoubleValue(200.0));

    Ptr<RandomRectanglePositionAllocator> positionAlloc =
        CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetX(xRv);
    positionAlloc->SetY(yRv);

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed",
                              PointerValue(speedRv),
                              "Pause",
                              PointerValue(pauseRv),
                              "PositionAllocator",
                              PointerValue(positionAlloc));
    mobility.Install(mobileNodes);

    LrWpanHelper lrwpan;
    NetDeviceContainer devices = lrwpan.Install(mobileNodes);
    lrwpan.CreateAssociatedPan(devices, 0);

    BasicEnergySourceHelper sourceHelper;
    sourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    sourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));
    energy::EnergySourceContainer sources = sourceHelper.Install(mobileNodes);

    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<energy::SimpleDeviceEnergyModel> model = CreateObject<energy::SimpleDeviceEnergyModel>();
        model->SetEnergySource(sources.Get(i));
        model->SetNode(mobileNodes.Get(i));
        model->SetCurrentA(0.000426);
        sources.Get(i)->AppendDeviceEnergyModel(model);
        energyModels.emplace(MakeEnergyKey(mobileNodes.Get(i)->GetId(), devices.Get(i)->GetIfIndex()),
                             model);
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::lrwpan::LrWpanNetDevice/Phy/TrxStateValue",
                    MakeCallback(&UpdateLrWpanCurrent));

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixDevices = sixlowpan.Install(devices);

    RipNgHelper ripNgRouting;
    Ipv6ListRoutingHelper listRouting;
    listRouting.Add(ripNgRouting, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(mobileNodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixDevices);

    InstallTcpFlowsIpv6(
        mobileNodes, interfaces, packetSize, flowCount, packetsPerSecond, 5.0, simTime);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    monitor->CheckForLostPackets();

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

    for (const auto& kv : energyModels)
    {
        metrics.totalEnergyJ += kv.second->GetTotalEnergyConsumption();
    }

    metrics.throughputKbps = (totalRxBytes * 8.0) / ((simTime - 5.0) * 1000.0);
    metrics.averageDelayMs =
        totalRxPackets > 0 ? (totalDelaySeconds * 1000.0) / totalRxPackets : 0.0;
    metrics.deliveryRatio =
        totalTxPackets > 0 ? static_cast<double>(totalRxPackets) / totalTxPackets : 0.0;
    metrics.dropRatio =
        totalTxPackets > 0 ? static_cast<double>(totalLostPackets) / totalTxPackets : 0.0;

    Simulator::Destroy();
    return metrics;
}

} // namespace submissioncase4

#endif
