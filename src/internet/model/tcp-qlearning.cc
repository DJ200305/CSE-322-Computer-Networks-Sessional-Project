#include "tcp-qlearning.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <numeric>
namespace ns3
{
    NS_LOG_COMPONENT_DEFINE ("TcpQlearning");
    NS_OBJECT_ENSURE_REGISTERED (TcpQlearning);
    TypeId
    TcpQlearning::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::TcpQlearning")
            .SetParent<RttEstimator> ()
            .SetGroupName("Internet")
            .AddConstructor<TcpQlearning> ()
            .AddAttribute("UseFuzzyLogic",
                          "Enable fuzzy-logic adaptation of alpha and gamma.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TcpQlearning::m_useFuzzyLogic),
                          MakeBooleanChecker())
            .AddAttribute("EnableMetricTrace",
                          "Write raw and normalized QRTT metrics to a text file.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TcpQlearning::m_enableMetricTrace),
                          MakeBooleanChecker())
            .AddAttribute("MetricTraceFile",
                          "Output file for QRTT metric traces.",
                          StringValue("tcp-qlearning-metrics.txt"),
                          MakeStringAccessor(&TcpQlearning::m_metricTraceFile),
                          MakeStringChecker())
        ;
        return tid;
    }
    TcpQlearning::TcpQlearning ()
    {
        NS_LOG_FUNCTION (this);
        state = SUCCESS;
        q_success = 1.0;
        q_failure = 1.0;
        alpha_s = 0.5;
        alpha_f = 0.4;
        gamma_s = -0.2;
        gamma_f = 0.1;
        m_useFuzzyLogic = false;
        m_enableMetricTrace = false;
        m_metricTraceFile = "tcp-qlearning-metrics.txt";
        m_currentAlpha = alpha_s;
        m_currentGamma = gamma_f;
        m_warmupSamples = 20;
        m_windowSize = 10;
        m_varianceBuckets.minValue = 0.0;
        m_varianceBuckets.maxValue = 1.0;
        m_lossBuckets.minValue = 0.0;
        m_lossBuckets.maxValue = 1.0;
        m_trendBuckets.minValue = -0.25;
        m_trendBuckets.maxValue = 0.25;
        m_varianceP25 = 0.05;
        m_varianceP50 = 0.10;
        m_varianceP75 = 0.20;
        m_lossP10 = 0.05;
        m_lossP25 = 0.15;
        m_lossP50 = 0.30;
        m_trendP25 = -0.02;
        m_trendP50 = 0.0;
        m_trendP75 = 0.02;
    }
    TcpQlearning::TcpQlearning (const TcpQlearning& r)
      : RttEstimator (r)
    {
        NS_LOG_FUNCTION (this);
        state = r.state;
        q_success = r.q_success;
        q_failure = r.q_failure;
        alpha_s = r.alpha_s;
        alpha_f = r.alpha_f;
        gamma_s = r.gamma_s;
        gamma_f = r.gamma_f;
        m_useFuzzyLogic = r.m_useFuzzyLogic;
        m_enableMetricTrace = r.m_enableMetricTrace;
        m_metricTraceFile = r.m_metricTraceFile;
        m_currentAlpha = r.m_currentAlpha;
        m_currentGamma = r.m_currentGamma;
        m_warmupSamples = r.m_warmupSamples;
        m_windowSize = r.m_windowSize;
        m_varianceMoments = r.m_varianceMoments;
        m_lossMoments = r.m_lossMoments;
        m_trendMoments = r.m_trendMoments;
        m_varianceBuckets = r.m_varianceBuckets;
        m_lossBuckets = r.m_lossBuckets;
        m_trendBuckets = r.m_trendBuckets;
        m_varianceP25 = r.m_varianceP25;
        m_varianceP50 = r.m_varianceP50;
        m_varianceP75 = r.m_varianceP75;
        m_lossP10 = r.m_lossP10;
        m_lossP25 = r.m_lossP25;
        m_lossP50 = r.m_lossP50;
        m_trendP25 = r.m_trendP25;
        m_trendP50 = r.m_trendP50;
        m_trendP75 = r.m_trendP75;
        m_recentRtt = r.m_recentRtt;
        m_recentAckDelay = r.m_recentAckDelay;
        m_recentTxEvents = r.m_recentTxEvents;
        m_recentTimeoutEvents = r.m_recentTimeoutEvents;
    }
    Ptr<RttEstimator>
    TcpQlearning::Copy () const
    {
        return CopyObject<TcpQlearning> (this);
    }
    Time
    TcpQlearning::GetRTO() const
    {
        double rto = (state == SUCCESS) ? q_success : q_failure;
        rto = std::max(0.2, std::min(rto, 60.0));
        return Seconds(rto);
    }
    void
    TcpQlearning::PushSample(SampleWindow& window, double value)
    {
        window.push_back(value);
        while (window.size() > m_windowSize)
        {
            window.pop_front();
        }
    }
    void
    TcpQlearning::PushEvent(EventWindow& window, uint8_t value)
    {
        window.push_back(value);
        while (window.size() > m_windowSize)
        {
            window.pop_front();
        }
    }
    void
    TcpQlearning::UpdateBucket(BucketEstimator& estimator, double sample)
    {
        const double clamped = std::clamp(sample, estimator.minValue, estimator.maxValue);
        const double width = estimator.maxValue - estimator.minValue;
        uint32_t index = 0;
        if (width > 0.0)
        {
            const double scaled = (clamped - estimator.minValue) / width;
            index = std::min(static_cast<uint32_t>(scaled * HIST_BUCKETS), HIST_BUCKETS - 1);
        }
        estimator.counts[index]++;
        estimator.total++;
    }
    double
    TcpQlearning::EstimateQuantile(const BucketEstimator& estimator, double q) const
    {
        if (estimator.total == 0)
        {
            return estimator.minValue;
        }
        const uint64_t target = static_cast<uint64_t>(std::ceil(q * estimator.total));
        uint64_t cumulative = 0;
        for (uint32_t i = 0; i < HIST_BUCKETS; ++i)
        {
            cumulative += estimator.counts[i];
            if (cumulative >= target)
            {
                const double bucketWidth = (estimator.maxValue - estimator.minValue) / static_cast<double>(HIST_BUCKETS);
                return estimator.minValue + bucketWidth * (static_cast<double>(i) + 0.5);
            }
        }
        return estimator.maxValue;
    }
    void
    TcpQlearning::UpdateDynamicThresholds(double rawStdDev, double rawLoss, double rawTrend)
    {
        UpdateBucket(m_varianceBuckets, rawStdDev);
        UpdateBucket(m_lossBuckets, rawLoss);
        UpdateBucket(m_trendBuckets, rawTrend);
        if (!ThresholdsReady())
        {
            return;
        }
        m_varianceP25 = EstimateQuantile(m_varianceBuckets, 0.25);
        m_varianceP50 = EstimateQuantile(m_varianceBuckets, 0.50);
        m_varianceP75 = EstimateQuantile(m_varianceBuckets, 0.75);
        m_lossP10 = EstimateQuantile(m_lossBuckets, 0.10);
        m_lossP25 = EstimateQuantile(m_lossBuckets, 0.25);
        m_lossP50 = EstimateQuantile(m_lossBuckets, 0.50);
        m_trendP25 = EstimateQuantile(m_trendBuckets, 0.25);
        m_trendP50 = EstimateQuantile(m_trendBuckets, 0.50);
        m_trendP75 = EstimateQuantile(m_trendBuckets, 0.75);
    }
    bool
    TcpQlearning::ThresholdsReady() const
    {
        return m_varianceBuckets.total >= m_warmupSamples && m_lossBuckets.total >= m_warmupSamples && m_trendBuckets.total >= m_warmupSamples;
    }
    double
    TcpQlearning::ComputeRttStdDev() const
    {
        if (m_recentRtt.size() < 2)
        {
            return 0.0;
        }
        const double mean = std::accumulate(m_recentRtt.begin(),m_recentRtt.end(),0.0)/static_cast<double>(m_recentRtt.size());
        double variance = 0.0;
        for (double sample:m_recentRtt)
        {
            const double diff = sample - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(m_recentRtt.size());
        return std::sqrt(variance);
    }
    double
    TcpQlearning::ComputeLossRate() const
    {
        if (m_recentTxEvents.empty())
        {
            return 0.0;
        }
        const double txCount = std::accumulate(m_recentTxEvents.begin(), m_recentTxEvents.end(), 0.0);
        const double timeoutCount = std::accumulate(m_recentTimeoutEvents.begin(), m_recentTimeoutEvents.end(), 0.0);
        if (txCount <= 0.0)
        {
            return 0.0;
        }
        return std::min(timeoutCount/txCount, 1.0);
    }
    double
    TcpQlearning::ComputeAckDelayTrend() const
    {
        if (m_recentAckDelay.size() < 2)
        {
            return 0.0;
        }
        const double n = static_cast<double>(m_recentAckDelay.size());
        const double meanX = (n - 1.0) / 2.0;
        const double meanY = std::accumulate(m_recentAckDelay.begin(), m_recentAckDelay.end(), 0.0) / n;
        double numer = 0.0;
        double denom = 0.0;
        for (size_t i = 0; i < m_recentAckDelay.size(); ++i)
        {
            const double x = static_cast<double>(i) - meanX;
            const double y = m_recentAckDelay[i] - meanY;
            numer += x*y;
            denom += x*x;
        }
        if (denom <= 0.0)
        {
            return 0.0;
        }
        return numer/denom;
    }
    double
    TcpQlearning::NormalizeVariance(double stddev) const
    {
        const double span = std::max(m_varianceP75 - m_varianceP25, 1e-9);
        return std::clamp((stddev - m_varianceP25) / span, 0.0, 1.0);
    }
    double
    TcpQlearning::NormalizeLoss(double lossRate) const
    {
        return std::clamp(lossRate, 0.0, 1.0);
    }
    double
    TcpQlearning::NormalizeTrend(double trend) const
    {
        const double span = std::max(m_trendP75 - m_trendP25, 1e-9);
        return std::clamp((trend - m_trendP25) / span, 0.0, 1.0);
    }
    double
    TcpQlearning::DescendingMembership(double x, double fullValue, double zeroValue) const
    {
        if (x <= fullValue)
        {
            return 1.0;
        }
        if (x >= zeroValue)
        {
            return 0.0;
        }
        return (zeroValue-x) / (zeroValue-fullValue);
    }
    double
    TcpQlearning::AscendingMembership(double x, double zeroValue, double fullValue) const
    {
        if (x <= zeroValue)
        {
            return 0.0;
        }
        if (x >= fullValue)
        {
            return 1.0;
        }
        return (x-zeroValue) / (fullValue-zeroValue);
    }
    double
    TcpQlearning::TriangularMembership(double x, double left, double peak, double right) const
    {
        if (x <= left || x >= right)
        {
            return 0.0;
        }
        if (x == peak)
        {
            return 1.0;
        }
        if (x < peak)
        {
            return (x-left) / (peak-left);
        }
        return (right-x) / (right-peak);
    }
    double
    TcpQlearning::MembershipLow(double x) const
    {
        return DescendingMembership(x, 0.25, 0.50);
    }
    double
    TcpQlearning::MembershipMedium(double x) const
    {
        return TriangularMembership(x, 0.25, 0.50, 0.75);
    }
    double
    TcpQlearning::MembershipHigh(double x) const
    {
        return AscendingMembership(x, 0.50, 0.75);
    }
    double
    TcpQlearning::InferAlpha(double varianceNorm, double trendNorm) const
    {
        const double varianceLow = DescendingMembership(varianceNorm, 0.0, NormalizeVariance(m_varianceP50));
        const double varianceMedium = TriangularMembership(varianceNorm,NormalizeVariance(m_varianceP25),NormalizeVariance(m_varianceP50),NormalizeVariance(m_varianceP75));
        const double varianceHigh = AscendingMembership(varianceNorm,NormalizeVariance(m_varianceP50),NormalizeVariance(m_varianceP75));
        const double trendFalling = DescendingMembership(trendNorm, 0.0, NormalizeTrend(m_trendP50));
        const double trendStable = TriangularMembership(trendNorm,NormalizeTrend(m_trendP25),NormalizeTrend(m_trendP50),NormalizeTrend(m_trendP75));
        const double trendRising = AscendingMembership(trendNorm,NormalizeTrend(m_trendP50),NormalizeTrend(m_trendP75));

        constexpr double alphaLow = 0.08;
        constexpr double alphaMedium = 0.20;
        constexpr double alphaHigh = 0.38;
        const double r1 = varianceHigh;
        const double r2 = trendRising;
        const double r3 = std::min(varianceMedium, trendRising);
        const double r4 = std::min(varianceMedium, trendStable);
        const double r5 = std::min(varianceLow, trendStable);
        const double r6 = std::min(varianceLow, trendFalling);
        const double numer = r1*alphaHigh + r2*alphaHigh + r3*alphaHigh + r4*alphaMedium + r5*alphaLow + r6*alphaLow;
        const double denom = r1 + r2 + r3 + r4 + r5 + r6;
        if (denom <= 0.0)
        {
            return alpha_s;
        }
        return numer/denom;
    }
    double
    TcpQlearning::InferGamma(double varianceNorm, double lossNorm, double trendNorm) const
    {
        const double lossLow = DescendingMembership(lossNorm,NormalizeLoss(m_lossP10),NormalizeLoss(m_lossP25));
        const double lossMedium = TriangularMembership(lossNorm,NormalizeLoss(m_lossP10),NormalizeLoss(m_lossP25),NormalizeLoss(m_lossP50));
        const double lossHigh = AscendingMembership(lossNorm,NormalizeLoss(m_lossP25),NormalizeLoss(m_lossP50));
        const double varianceLow = DescendingMembership(varianceNorm, 0.0, NormalizeVariance(m_varianceP50));
        const double varianceMedium = TriangularMembership(varianceNorm,NormalizeVariance(m_varianceP25),NormalizeVariance(m_varianceP50),NormalizeVariance(m_varianceP75));
        const double varianceHigh = AscendingMembership(varianceNorm,NormalizeVariance(m_varianceP50),NormalizeVariance(m_varianceP75));
        const double trendFalling = DescendingMembership(trendNorm, 0.0, NormalizeTrend(m_trendP50));
        const double trendStable = TriangularMembership(trendNorm,NormalizeTrend(m_trendP25),NormalizeTrend(m_trendP50),NormalizeTrend(m_trendP75));
        const double trendRising = AscendingMembership(trendNorm,NormalizeTrend(m_trendP50),NormalizeTrend(m_trendP75));

        const double loadNorm = lossNorm;
        const double gammaSmall = -0.15 - 0.20*loadNorm;
        const double gammaMedium = 0.05 - 0.20*loadNorm;
        const double gammaLarge = 0.20 - 0.15*loadNorm;

        const double r1 = lossHigh;
        const double r2 = std::min(lossMedium, trendRising);
        const double r3 = std::min(varianceHigh, lossMedium);
        const double r4 = std::min(std::min(lossLow, varianceLow), trendStable);
        const double r5 = std::min(lossLow, varianceMedium);
        const double r6 = std::min(lossLow, trendFalling);

        const double numer = r1*gammaSmall + r2*gammaSmall + r3*gammaSmall + r4*gammaLarge + r5*gammaMedium + r6*gammaLarge;
        const double denom = r1 + r2 + r3 + r4 + r5 + r6;
        if (denom <= 0.0)
        {
            return gamma_f;
        }
        return numer/denom;
    }
    void
    TcpQlearning::TraceMetrics(double rawStdDev,
                               double rawLoss,
                               double rawTrend,
                               double varianceNorm,
                               double lossNorm,
                               double trendNorm) const
    {
        std::ofstream trace(m_metricTraceFile, std::ios::app);
        if (!trace.is_open())
        {
            return;
        }
        trace << Simulator::Now().GetSeconds() << " "
              << rawStdDev << " "
              << rawLoss << " "
              << rawTrend << " "
              << varianceNorm << " "
              << lossNorm << " "
              << trendNorm << " "
              << m_currentAlpha << " "
              << m_currentGamma << "\n";
    }
    void
    TcpQlearning::UpdateFuzzyParameters()
    {
        const double rawStdDev = ComputeRttStdDev();
        const double rawLoss = ComputeLossRate();
        const double rawTrend = ComputeAckDelayTrend();
        UpdateDynamicThresholds(rawStdDev, rawLoss, rawTrend);
        const double varianceNorm = NormalizeVariance(rawStdDev);
        const double lossNorm = NormalizeLoss(rawLoss);
        const double trendNorm = NormalizeTrend(rawTrend);
        if (m_useFuzzyLogic && ThresholdsReady())
        {
            m_currentAlpha = std::clamp(InferAlpha(varianceNorm, trendNorm), 0.05, 0.45);
            m_currentGamma = std::clamp(InferGamma(varianceNorm, lossNorm, trendNorm), -0.5, 0.25);
        }
        if (m_enableMetricTrace)
        {
            TraceMetrics(rawStdDev, rawLoss, rawTrend, varianceNorm, lossNorm, trendNorm);
        }
        NS_LOG_DEBUG("Fuzzy QRTT metrics varianceNorm=" << varianceNorm
                     << " lossNorm=" << lossNorm
                     << " trendNorm=" << trendNorm
                     << " alpha=" << m_currentAlpha
                     << " gamma=" << m_currentGamma);
    }
    void 
    TcpQlearning::Measurement(Time t)
    {
        double delay = t.GetSeconds();
        if (m_useFuzzyLogic || m_enableMetricTrace)
        {
            PushSample(m_recentRtt, delay);
            PushSample(m_recentAckDelay, delay);
            UpdateFuzzyParameters();
        }
        STATE prevState = state;
        if (!m_useFuzzyLogic)
        {
            m_currentAlpha = (prevState == SUCCESS) ? alpha_s : alpha_f;
            m_currentGamma = (prevState == SUCCESS) ? gamma_s : gamma_f;
        }
        else if (!ThresholdsReady())
        {
            m_currentAlpha = (prevState == SUCCESS) ? alpha_s : alpha_f;
            m_currentGamma = (prevState == SUCCESS) ? gamma_s : gamma_f;
        }
        double nextBestQ = std::max(q_success, q_failure);
        double& currentQ = (prevState == SUCCESS) ? q_success : q_failure;
        currentQ = currentQ + m_currentAlpha * (delay + m_currentGamma * nextBestQ - currentQ);
        //currentQ = std::max(0.2, std::min(currentQ, 60.0));
        if(!m_useFuzzyLogic)
        {
            currentQ = std::max(0.2, std::min(currentQ, 60.0));
        }
        state = SUCCESS;
        m_estimatedRtt = Seconds(currentQ);
        m_estimatedVariation = (m_useFuzzyLogic || m_enableMetricTrace) ? Seconds(ComputeRttStdDev()) : Time(0);
        m_nSamples++;
    }
    void 
    TcpQlearning::RecordTimeout(Time t)
    {
        double delay = t.GetSeconds();
        if (m_useFuzzyLogic || m_enableMetricTrace)
        {
            PushEvent(m_recentTxEvents,1);
            PushEvent(m_recentTimeoutEvents,1);
            PushSample(m_recentAckDelay,delay);
            UpdateFuzzyParameters();
        }

        STATE prevState = state;
        if (!m_useFuzzyLogic)
        {
            m_currentAlpha = (prevState == SUCCESS) ? alpha_s : alpha_f;
            m_currentGamma = (prevState == SUCCESS) ? gamma_s : gamma_f;
        }
        else if (!ThresholdsReady())
        {
            m_currentAlpha = (prevState == SUCCESS) ? alpha_s : alpha_f;
            m_currentGamma = (prevState == SUCCESS) ? gamma_s : gamma_f;
        }
        double nextBestQ = std::max(q_success, q_failure);
        double& currentQ = (prevState == SUCCESS) ? q_success : q_failure;
        currentQ = currentQ + m_currentAlpha * (delay + m_currentGamma * nextBestQ - currentQ);
        //currentQ = std::max(0.2, std::min(currentQ, 60.0));
        if(!m_useFuzzyLogic){
            currentQ = std::max(0.2, std::min(currentQ, 60.0));
        }
        state = FAILURE;
        m_estimatedRtt = Seconds(currentQ);
        m_estimatedVariation = (m_useFuzzyLogic || m_enableMetricTrace) ? Seconds(ComputeRttStdDev()) : Time(0);
    }
    void 
    TcpQlearning::SentSeq(SequenceNumber32 seq, uint32_t size)
    {
        (void)seq;
        if (size > 0)
        {
            PushEvent(m_recentTxEvents, 1);
        }
    }
    void 
    TcpQlearning::Reset()
    {
        state = SUCCESS;
        q_success = 1.0;
        q_failure = 1.0;
        m_currentAlpha = alpha_s;
        m_currentGamma = gamma_f;
        m_varianceMoments = OnlineMoments{};
        m_lossMoments = OnlineMoments{};
        m_trendMoments = OnlineMoments{};
        m_varianceBuckets = BucketEstimator{0.0, 1.0, 0, {}};
        m_lossBuckets = BucketEstimator{0.0, 1.0, 0, {}};
        m_trendBuckets = BucketEstimator{-0.25, 0.25, 0, {}};
        m_varianceP25 = 0.05;
        m_varianceP50 = 0.10;
        m_varianceP75 = 0.20;
        m_lossP10 = 0.05;
        m_lossP25 = 0.15;
        m_lossP50 = 0.30;
        m_trendP25 = -0.02;
        m_trendP50 = 0.0;
        m_trendP75 = 0.02;
        m_recentRtt.clear();
        m_recentAckDelay.clear();
        m_recentTxEvents.clear();
        m_recentTimeoutEvents.clear();
        m_estimatedRtt = Time(1.0);
        m_estimatedVariation = Time(0);
        m_nSamples = 0;
    }
}
