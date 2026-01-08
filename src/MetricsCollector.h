//
// Metrics Collector for ODA-MD Simulation
// Collects DA (Detection Accuracy), FAR (False Alarm Rate), and other metrics
//

#ifndef __ODAMD_METRICSCOLLECTOR_H_
#define __ODAMD_METRICSCOLLECTOR_H_

#include <omnetpp.h>
#include <fstream>
#include <iomanip>

using namespace omnetpp;

class MetricsCollector {
  private:
    // Confusion Matrix values
    int truePositives;   // Correctly detected outliers
    int falsePositives;  // Normal data incorrectly flagged as outlier
    int trueNegatives;   // Correctly identified normal data
    int falseNegatives;  // Outliers missed (not detected)

    // Thời gian của các mẫu
    std::vector<simtime_t> logTimes;
    std::vector<double> logDA;
    std::vector<double> logFAR;

  public:
    MetricsCollector() {
        reset();
    }

    void reset() {
        truePositives = 0;
        falsePositives = 0;
        trueNegatives = 0;
        falseNegatives = 0;
        logTimes.clear();
        logDA.clear();
        logFAR.clear();
        logTP.clear();
        logFP.clear();
    }

    // Record a detection result
    void recordDetection(bool actualOutlier, bool detectedAsOutlier) {
        if (actualOutlier && detectedAsOutlier) {
            truePositives++;
        } else if (!actualOutlier && detectedAsOutlier) {
            falsePositives++;
        } else if (!actualOutlier && !detectedAsOutlier) {
            trueNegatives++;
        } else if (actualOutlier && !detectedAsOutlier) {
            falseNegatives++;
        }
    }

    // Detection Accuracy = TP / (TP + FN)
    double getDetectionAccuracy() const {
        int denominator = truePositives + falseNegatives;
        if (denominator == 0) return 0.0;
        return (double)truePositives / denominator;
    }

    // False Alarm Rate = FP / (FP + TN)
    double getFalseAlarmRate() const {
        int denominator = falsePositives + trueNegatives;
        if (denominator == 0) return 0.0;
        return (double)falsePositives / denominator;
    }

    // Precision = TP / (TP + FP)
    double getPrecision() const {
        int denominator = truePositives + falsePositives;
        if (denominator == 0) return 0.0;
        return (double)truePositives / denominator;
    }

    // Log current metrics at a specific time
    void logMetrics(simtime_t time) {
        logTimes.push_back(time);
        logDA.push_back(getDetectionAccuracy());
        logFAR.push_back(getFalseAlarmRate());
        // Also log cumulative counts for paper-style graphs
        logTP.push_back(truePositives);
        logFP.push_back(falsePositives);
    }
    
    std::vector<int> logTP;   // Cumulative TP over time
    std::vector<int> logFP;   // Cumulative FP over time

    // Get statistics
    int getTP() const { return truePositives; }
    int getFP() const { return falsePositives; }
    int getTN() const { return trueNegatives; }
    int getFN() const { return falseNegatives; }
    int getTotalSamples() const { return truePositives + falsePositives + trueNegatives + falseNegatives; }
    int getTotalOutliersDetected() const { return truePositives + falsePositives; }
    int getActualOutliers() const { return truePositives + falseNegatives; }

    // Export metrics to CSV file (with cumulative data for paper-style graphs)
    void exportToCSV(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        // Header: Time, DA%, FAR%, Cumulative TP, Cumulative FP
        file << "Time,DA,FAR,CumulativeTP,CumulativeFP\n";
        for (size_t i = 0; i < logTimes.size(); i++) {
            file << std::fixed << std::setprecision(2) << logTimes[i].dbl()
                 << "," << std::setprecision(4) << logDA[i]
                 << "," << logFAR[i]
                 << "," << logTP[i]
                 << "," << logFP[i] << "\n";
        }

        file.close();
    }

    // Print summary
    void printSummary() const {
        EV << "========================================\n";
        EV << "       METRICS SUMMARY (ODA-MD)\n";
        EV << "========================================\n";
        EV << "Confusion Matrix:\n";
        EV << "  True Positives (TP):  " << truePositives << "\n";
        EV << "  False Positives (FP): " << falsePositives << "\n";
        EV << "  True Negatives (TN):  " << trueNegatives << "\n";
        EV << "  False Negatives (FN): " << falseNegatives << "\n";
        EV << "----------------------------------------\n";
        EV << "Detection Accuracy (DA): " << std::fixed << std::setprecision(4)
           << (getDetectionAccuracy() * 100) << "%\n";
        EV << "False Alarm Rate (FAR):  " << (getFalseAlarmRate() * 100) << "%\n";
        EV << "Precision:               " << (getPrecision() * 100) << "%\n";
        EV << "Total Samples Processed: " << getTotalSamples() << "\n";
        EV << "========================================\n";
    }
};

#endif
