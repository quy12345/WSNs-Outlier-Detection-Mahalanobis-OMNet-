//
// ClusterHead - Implements ODA-MD and OD (baseline) algorithms
// Simplified version matching paper's approach (no training phase)
//

#ifndef __ODAMD_CLUSTERHEAD_H_
#define __ODAMD_CLUSTERHEAD_H_

#include <omnetpp.h>
#include <vector>
#include "messages_m.h"
#include "MetricsCollector.h"
#include "EnergyModel.h"
#include "IntelLabData.h"

using namespace omnetpp;

enum Algorithm {
    ALG_ODA_MD,
    ALG_OD
};

class ClusterHead : public cSimpleModule
{
  private:
    int clusterSize;
    double threshold;
    Algorithm algorithm;
    int chMoteId;

    std::vector<SensorMsg *> dataBuffer;

    IntelLabData* chData;
    bool dataLoaded;

    MetricsCollector metrics;
    EnergyModel energy;

    int totalPacketsReceived;
    int totalOutliersDetected;
    int totalPacketsForwarded;

    cMessage *logTimer;
    double logInterval;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void loadCHData();
    void addCHReading();

    void runODAMD();
    void runOD();

    std::vector<double> calculateMean(const std::vector<std::vector<double>>& data);
    std::vector<std::vector<double>> calculateCovariance(const std::vector<std::vector<double>>& data, const std::vector<double>& mean);
    bool invertMatrix4x4(const std::vector<std::vector<double>>& matrix, std::vector<std::vector<double>>& inverse);
    double calculateMahalanobis(const std::vector<double>& sample, const std::vector<double>& mean, const std::vector<std::vector<double>>& invCov);
    double calculateEuclidean(const std::vector<double>& sample, const std::vector<double>& mean);
};

#endif
