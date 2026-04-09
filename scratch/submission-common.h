#ifndef SUBMISSION_COMMON_H
#define SUBMISSION_COMMON_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/tcp-qlearning.h"

#include <string>
#include <utility>
#include <vector>

namespace submissioncase4
{
using namespace ns3;

struct Metrics
{
    double throughputKbps = 0.0;
    double averageDelayMs = 0.0;
    double deliveryRatio = 0.0;
    double dropRatio = 0.0;
    double totalEnergyJ = 0.0;
};

inline std::vector<std::pair<uint32_t, uint32_t>>
BuildFlows(uint32_t nodes, uint32_t flowCount)
{
    std::vector<std::pair<uint32_t, uint32_t>> flows;
    flows.reserve(flowCount);
    for (uint32_t i = 0; i < flowCount; ++i)
    {
        uint32_t src = i % nodes;
        uint32_t dst = (src + (nodes / 2)) % nodes;
        if (src == dst)
        {
            dst = (dst + 1) % nodes;
        }
        flows.emplace_back(src, dst);
    }
    return flows;
}

inline void
ConfigureQrtt(bool useQrtt, bool useFuzzyQrtt)
{
    if (!useQrtt && !useFuzzyQrtt)
    {
        std::cout<<"Using Jacobson"<<std::endl;
        return;
    }
    if(useQrtt)
    {
        std::cout<<"Using QRTT"<<std::endl;
    }
    if(useFuzzyQrtt)
    {
        std::cout<<"Using Fuzzy QRTT"<<std::endl;
    }
    Config::SetDefault("ns3::TcpL4Protocol::RttEstimatorType",
                       TypeIdValue(TcpQlearning::GetTypeId()));
    Config::SetDefault("ns3::TcpQlearning::UseFuzzyLogic", BooleanValue(useFuzzyQrtt));
}

} // namespace submissioncase4

#endif
