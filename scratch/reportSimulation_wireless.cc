#include "submission-common.h"
#include "submission-lrwpan-mobile.h"

#include <fstream>
#include <string>
#include <vector>

using namespace ns3;
using namespace submissioncase4;

NS_LOG_COMPONENT_DEFINE("SubmissionCase4MobileBatch");

int
main(int argc, char* argv[])
{
    uint32_t packetSize = 128;
    double simTime = 120.0;
    bool useQrtt = false;
    bool useFuzzyQrtt = false;
    std::string resultsFile = "scratch/wired_report_Jacobson.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Application packet size in bytes", packetSize);
    cmd.AddValue("simTime", "Simulation stop time in seconds", simTime);
    cmd.AddValue("useQrtt", "Use QRTT instead of Jacobson", useQrtt);
    cmd.AddValue("useFuzzyQrtt", "Enable fuzzy logic inside QRTT", useFuzzyQrtt);
    cmd.AddValue("resultsFile", "CSV output path", resultsFile);
    cmd.Parse(argc, argv);

    // 15 representative combinations: low / mid / high sweeps across all parameters
    struct Combo { uint32_t nodes, flows, pps; double speed; };
    const std::vector<Combo> combos = {
        // --- Low load ---
        { 20,  10, 100,  5.0},
        { 20,  10, 100, 25.0},
        { 20,  10, 500,  5.0},
        // --- Medium nodes, varying flows & pps ---
        { 60,  10, 100, 15.0},
        { 60,  20, 200, 10.0},
        { 60,  30, 300, 15.0},
        { 60,  40, 400, 20.0},
        { 60,  50, 500, 25.0},
        // --- High load ---
        {100,  50, 500,  5.0},
        {100,  50, 500, 25.0},
        {100,  10, 100, 25.0},
        // --- Diagonal sweep (all params scale together) ---
        { 20,  10, 100,  5.0},
        { 40,  20, 200, 10.0},
        { 60,  30, 300, 15.0},
        { 80,  40, 400, 20.0},
    };

    std::ofstream out(resultsFile, std::ios::trunc);
    out << "nodes,flows,packets_per_second,speed_mps,throughput_kbps,avg_delay_ms,"
           "delivery_ratio,drop_ratio,mode\n";

    std::string mode = "jacobson";
    if (useFuzzyQrtt)      mode = "fuzzy_qrtt";
    else if (useQrtt)      mode = "qrtt";

    uint32_t runIndex = 0;
    for (const auto& c : combos)
    {
        ++runIndex;
        std::cout << "Running combination " << runIndex << "/" << combos.size() << ": "
                  << "nodes=" << c.nodes << ", "
                  << "flows=" << c.flows << ", "
                  << "pps="   << c.pps   << ", "
                  << "speed=" << c.speed << std::endl;

        Metrics metrics = RunLrWpanMobileSimulation(c.nodes,
                                                   c.flows,
                                                   c.pps,
                                                   packetSize,
                                                   c.speed,
                                                   simTime,
                                                   useQrtt,
                                             useFuzzyQrtt);

        out << c.nodes << ","
            << c.flows << ","
            << c.pps   << ","
            << c.speed << ","
            << metrics.throughputKbps << ","
            << metrics.averageDelayMs << ","
            << metrics.deliveryRatio  << ","
            << metrics.dropRatio      << ","
            << mode << "\n";
        out.flush();
    }

    out.close();
    std::cout << "Saved CSV results to " << resultsFile << std::endl;
    return 0;
}