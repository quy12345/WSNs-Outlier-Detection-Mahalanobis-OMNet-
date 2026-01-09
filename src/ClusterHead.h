//
// ClusterHead - Implements ODA-MD and OD (baseline) algorithms
// Simplified version matching paper's approach (no training phase)
//

#ifndef __ODAMD_CLUSTERHEAD_H_
#define __ODAMD_CLUSTERHEAD_H_

#include <omnetpp.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include "messages_m.h"
#include "MetricsCollector.h"
#include "EnergyModel.h"
#include "IntelLabData.h"

using namespace omnetpp;

enum Algorithm {
    ALG_ODA_MD,
    ALG_OD
};

// Sliding Window size for real-time processing
const int WINDOW_SIZE = 20;

class ClusterHead : public cSimpleModule
{
  private:
    double threshold;
    Algorithm algorithm;
    int chMoteId;

    // Sliding Window for real-time ODA-MD (replaces block batching)
    std::deque<SensorMsg *> slidingWindow;

    IntelLabData* chData;
    bool dataLoaded;

    MetricsCollector metrics;
    EnergyModel energy;

    int totalPacketsReceived;
    int totalOutliersDetected;
    int totalPacketsForwarded;
    
    // Flag to track if initial window has been processed
    bool isInitialWindowProcessed;

    cMessage *logTimer;
    double logInterval;
    
    // Request-Response pattern (Algorithm 1 - ODA-MD paper)
    cMessage *requestTimer;
    double requestInterval;
    int numSensors;
    int requestId;

    // =========================================================================
    // OD Algorithm (Fawzy et al., 2013) - Data Structures
    // =========================================================================
    struct DataCluster {
        std::vector<double> center;     // 4D center (T, H, L, V)
        std::vector<int> members;       // Indices of member points
        bool isOutlier;                 // Outlier cluster flag
        double avgInterClusterDist;     // Average distance to other clusters
    };
    
    std::vector<DataCluster> odClusters;    // Clusters for OD algorithm
    double clusterWidth;                     // Fixed-width clustering parameter
    std::map<int, int> sensorErrorCount;    // Error count per sensor (for trust)
    std::map<int, int> sensorTotalCount;    // Total readings per sensor

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    // Request-Response pattern
    void sendDataRequest();

    void loadCHData();
    void addCHReading();

    // ODA-MD Algorithm
    void runODAMD();
    
    // OD Algorithm (Fawzy et al.) - Full 4-Step
    void runOD();
    void runOD_Clustering(const std::vector<std::vector<double>>& X);
    void runOD_Detection();
    void runOD_Classification(const std::vector<std::vector<double>>& X);
    double getSensorTrust(int sensorId);

    // Math utilities
    std::vector<double> calculateMean(const std::vector<std::vector<double>>& data);
    std::vector<std::vector<double>> calculateCovariance(const std::vector<std::vector<double>>& data, const std::vector<double>& mean);
    bool invertMatrix4x4(const std::vector<std::vector<double>>& matrix, std::vector<std::vector<double>>& inverse);
    double calculateMahalanobis(const std::vector<double>& sample, const std::vector<double>& mean, const std::vector<std::vector<double>>& invCov);
    double calculateEuclidean(const std::vector<double>& sample, const std::vector<double>& mean);
};

#endif
