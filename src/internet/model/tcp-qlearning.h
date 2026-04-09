#ifndef TCP_QLEARNING_H
#define TCP_QLEARNING_H

#include <deque>
#include <array>
#include <string>

#include "ns3/rtt-estimator.h"
#include "ns3/nstime.h"
#include "ns3/sequence-number.h"
namespace ns3
{
    class TcpQlearning : public RttEstimator
    {
      public:
        static TypeId GetTypeId(void);
        TcpQlearning();
        Ptr<RttEstimator> Copy() const override;
        TcpQlearning(const TcpQlearning& r);
        void Measurement(Time t) override;
        virtual void SentSeq(SequenceNumber32 seq, uint32_t size);
        void Reset() override;
        void RecordTimeout(Time t);
        Time GetRTO() const override;
      private:
        using SampleWindow = std::deque<double>;
        using EventWindow = std::deque<uint8_t>;
        static constexpr uint32_t HIST_BUCKETS = 32;
        enum STATE
        {
            SUCCESS,
            FAILURE
        };
        struct OnlineMoments
        {
            uint64_t count = 0;
            double mean = 0.0;
            double m2 = 0.0;
        };
        struct BucketEstimator
        {
            double minValue = 0.0;
            double maxValue = 1.0;
            uint64_t total = 0;
            std::array<uint32_t, HIST_BUCKETS> counts{};
        };
        void PushSample(SampleWindow& window, double value);
        void PushEvent(EventWindow& window, uint8_t value);
        void UpdateMoments(OnlineMoments& stats, double sample);
        void UpdateBucket(BucketEstimator& estimator, double sample);
        double EstimateQuantile(const BucketEstimator& estimator, double q) const;
        void UpdateDynamicThresholds(double rawStdDev, double rawLoss, double rawTrend);
        bool ThresholdsReady() const;

        double ComputeRttStdDev() const;
        double ComputeLossRate() const;
        double ComputeAckDelayTrend() const;

        double NormalizeVariance(double stddev) const;
        double NormalizeLoss(double lossRate) const;
        double NormalizeTrend(double trend) const;

        double MembershipLow(double x) const;
        double MembershipMedium(double x) const;
        double MembershipHigh(double x) const;
        double DescendingMembership(double x, double fullValue, double zeroValue) const;
        double AscendingMembership(double x, double zeroValue, double fullValue) const;
        double TriangularMembership(double x, double left, double peak, double right) const;

        double InferAlpha(double varianceNorm, double trendNorm) const;
        double InferGamma(double varianceNorm, double lossNorm, double trendNorm) const;
        void TraceMetrics(double rawStdDev,
                          double rawLoss,
                          double rawTrend,
                          double varianceNorm,
                          double lossNorm,
                          double trendNorm) const;
        void UpdateFuzzyParameters();

        STATE state;
        double q_success;
        double q_failure;
        double alpha_s;
        double alpha_f;
        double gamma_s;
        double gamma_f;
        bool m_useFuzzyLogic;
        bool m_enableMetricTrace;
        std::string m_metricTraceFile;
        double m_currentAlpha;
        double m_currentGamma;
        uint32_t m_warmupSamples;
        uint32_t m_windowSize;
        OnlineMoments m_varianceMoments;
        OnlineMoments m_lossMoments;
        OnlineMoments m_trendMoments;
        BucketEstimator m_varianceBuckets;
        BucketEstimator m_lossBuckets;
        BucketEstimator m_trendBuckets;
        double m_varianceP25;
        double m_varianceP50;
        double m_varianceP75;
        double m_lossP10;
        double m_lossP25;
        double m_lossP50;
        double m_trendP25;
        double m_trendP50;
        double m_trendP75;
        //ignore below five variables, these were for testing other things, not part of the final design
        double m_emaStdDev;    
        double m_emaLoss;
        double m_emaTrend;
        double m_emaAlpha;   
        u_int32_t m_updateInterval;
        SampleWindow m_recentRtt;
        SampleWindow m_recentAckDelay;
        EventWindow m_recentTxEvents;
        EventWindow m_recentTimeoutEvents;
    };
}
#endif
