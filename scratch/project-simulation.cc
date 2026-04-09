#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/lr-wpan-phy.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-qlearning.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/lr-wpan-mac.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ripng-helper.h"
#include "ns3/energy-module.h"
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <vector>
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("ProjectSimulation");

namespace
{
std::unordered_map<uint64_t, Ptr<energy::SimpleDeviceEnergyModel>> g_energyModels;
uint64_t
MakeEnergyKey(uint32_t nodeId, uint32_t devId)
{
    return (static_cast<uint64_t>(nodeId) << 32) | devId;
}
void
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
    auto it = g_energyModels.find(MakeEnergyKey(nodeId, devId));
    if (it == g_energyModels.end())
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
        currentA = kIdleCurrentA;
        break;
    }
    it->second->SetCurrentA(currentA);
}
} 

int main (int argc, char *argv[])
{
    bool useQrtt = false;
    bool useFuzzyQrtt = false;
    bool traceQrttMetrics = false;
    std::string qrttMetricTraceFile = "tcp-qlearning-metrics.txt";
    CommandLine cmd;
    cmd.AddValue("useQrtt", "Use QRTT instead of Jacobson", useQrtt);
    cmd.AddValue("useFuzzyQrtt", "Enable fuzzy logic inside QRTT", useFuzzyQrtt);
    cmd.AddValue("traceQrttMetrics", "Write QRTT raw and normalized metrics to a text file", traceQrttMetrics);
    cmd.AddValue("qrttMetricTraceFile", "Output file for QRTT metric traces", qrttMetricTraceFile);
    cmd.Parse(argc, argv);

    std::string protocol = "Jacobson";
    if (useFuzzyQrtt)
    {
        protocol = "FuzzyQRTT";
    }
    else if (useQrtt)
    {
        protocol = "QRTT";
    }

    if (useQrtt || useFuzzyQrtt || traceQrttMetrics)
    {
        Config::SetDefault("ns3::TcpL4Protocol::RttEstimatorType",
                           TypeIdValue(TcpQlearning::GetTypeId()));
        Config::SetDefault("ns3::TcpQlearning::UseFuzzyLogic",
                           BooleanValue(useFuzzyQrtt));
        Config::SetDefault("ns3::TcpQlearning::EnableMetricTrace",
                           BooleanValue(traceQrttMetrics));
        Config::SetDefault("ns3::TcpQlearning::MetricTraceFile",
                           StringValue(qrttMetricTraceFile));
    }
    
    // Config::SetDefault ("ns3::TcpSocket::ConnCount",UintegerValue(10));
    // Config::SetDefault ("ns3::TcpSocket::DelAckTimeout",TimeValue(Seconds(0.5)));
    // Config::SetDefault ("ns3::TcpSocket::InitialCwnd",UintegerValue(1));
    NodeContainer nodes;
    bool enable_flowmon = true;
    nodes.Create(100);
    MobilityHelper mh;
    mh.SetPositionAllocator(
        "ns3::GridPositionAllocator",
        "MinX",DoubleValue(0.0),
        "MinY",DoubleValue(0.0),
        "DeltaX",DoubleValue(30.0),
        "DeltaY",DoubleValue(30.0),
        "GridWidth",UintegerValue(10),
        "LayoutType",StringValue("RowFirst")
    );
    mh.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mh.Install(nodes);
    LrWpanHelper lwh;
    NetDeviceContainer devices = lwh.Install(nodes);
    lwh.CreateAssociatedPan(devices,0);
    BasicEnergySourceHelper sourceHelper;
    sourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    sourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));
    energy::EnergySourceContainer sources = sourceHelper.Install(nodes);
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<energy::SimpleDeviceEnergyModel> model = CreateObject<energy::SimpleDeviceEnergyModel>();
        model->SetEnergySource(sources.Get(i));
        model->SetNode(nodes.Get(i));
        model->SetCurrentA(0.000426);
        sources.Get(i)->AppendDeviceEnergyModel(model);
        g_energyModels.emplace(MakeEnergyKey(nodes.Get(i)->GetId(), devices.Get(i)->GetIfIndex()), model);
    }
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::lrwpan::LrWpanNetDevice/Phy/TrxStateValue",
                    MakeCallback(&UpdateLrWpanCurrent));
    // Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    // em->SetAttribute("ErrorRate", DoubleValue(0.1)); 
    // em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    // for (uint32_t i = 0; i < devices.GetN(); ++i)
    // {
    //     Ptr<ns3::lrwpan::LrWpanNetDevice> dev = DynamicCast<ns3::lrwpan::LrWpanNetDevice>(devices.Get(i));
    //     dev->GetMac()->SetPanId(10);
    //     dev->GetMac()->SetShortAddress(Mac16Address((uint16_t)(i + 1)));
    //     dev->GetPhy()->SetAttribute("PostReceptionErrorModel", PointerValue(em));
    // }
    RipNgHelper ripNgRouting;
    Ipv6ListRoutingHelper listRouting;
    listRouting.Add(ripNgRouting,0);
    // InternetStackHelper internet;
    // internet.SetIpv4StackInstall(false);   
    // internet.SetRoutingHelper(listRouting);
    // internet.Install(nodes);
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixDevices = sixlowpan.Install(devices);
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(listRouting);
    internetv6.Install(nodes);
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixDevices);
    // for (uint32_t i = 0; i < interfaces.GetN(); ++i)
    // {
    //     interfaces.SetForwarding(i, true);
    // }
    uint32_t sz = 28;
    uint16_t basePort = 9000;
    uint16_t port = basePort;
    const std::vector<std::pair<uint32_t, uint32_t>> flows = {
    {0,22},{1,23},{2,24},{3,25},{4,26},
    {10,32},{11,33},{12,34},{13,35},{14,36},
    {20,42},{21,43},{22,44},{23,45},{24,46},
    {30,52},{31,53},{32,54},{33,55},{34,56},
};
    for (uint32_t i=0;i<20;i++)
    {
        //Ptr<UniformRandomVariable> obj = CreateObject<UniformRandomVariable>();
        uint32_t src = flows[i].first;
        uint32_t dst = flows[i].second;
        Address sinkLocalAddress = Inet6SocketAddress(Ipv6Address::GetAny(), port);
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(dst));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(200.0));
        Address sinkRemoteAddress = Inet6SocketAddress(interfaces.GetAddress(dst, 1), port);
        OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkRemoteAddress);
        clientHelper.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        clientHelper.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        clientHelper.SetAttribute("PacketSize",UintegerValue(sz));
        clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("1Mbps")));
        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(src));
        clientApp.Start(Seconds(15.0 + i * 0.5));
        clientApp.Stop(Seconds(200.0));
        port++;
    }
    FlowMonitorHelper flowmon;
    if (enable_flowmon)
        flowmon.InstallAll();
    Simulator::Stop(Seconds(200.0));
    Simulator::Run();
    if (enable_flowmon)
        flowmon.SerializeToXmlFile("project-flow-monitor.xml", false, false);
    Ptr<FlowMonitor> monitor = flowmon.GetMonitor();
    auto classifier = DynamicCast<Ipv6FlowClassifier>(flowmon.GetClassifier6());
    uint64_t totRxBytes = 0;
    uint64_t totTxPackets = 0;
    uint64_t totRxPackets = 0;
    uint64_t totLostPackets = 0;
    double totEnergymJ = 0.0;
    double averageEnergyPerBitmJ = 0.0;
    for (const auto& flow : monitor->GetFlowStats())
    {
        if (classifier)
        {
            Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
            if (t.destinationPort < basePort || t.destinationPort >= basePort + flows.size())
            {
                continue;
            }
        }
        const auto& st = flow.second;
        totRxBytes += st.rxBytes;
        totTxPackets += st.txPackets;
        totRxPackets += st.rxPackets;
        totLostPackets += st.lostPackets;
    }
    for (const auto& kv : g_energyModels)
    {
        totEnergymJ += kv.second->GetTotalEnergyConsumption();
    }
    double throughput = (totRxBytes*8.0)/(185.0*1000.0);
    double deliveryRatio = (totTxPackets > 0) ? (static_cast<double>(totRxPackets) / totTxPackets) : 0.0;
    double dropRatio = (totTxPackets > 0) ? (static_cast<double>(totLostPackets) / totTxPackets) : 0.0;
    double totRxBits = static_cast<double>(totRxBytes) * 8.0;
    averageEnergyPerBitmJ = (totRxBits > 0.0) ? (totEnergymJ / totRxBits) : 0.0;
    std::ofstream outfile("tcp-qlearning-results.txt", std::ios_base::app);
    if (outfile.is_open())
    {
        outfile << protocol << " "
                << throughput << " "
                << totTxPackets << " "
                << averageEnergyPerBitmJ << " "
                << totEnergymJ << " "
                << deliveryRatio << " "
                << dropRatio << std::endl;
        outfile.close();
        std::cout<<"Results saved to tcp-qlearning-results.txt"<<std::endl;
    }
    Simulator::Destroy();
    return 0;
}
