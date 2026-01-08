//
// ClusterHead - Implements ODA-MD and OD (baseline) algorithms
// Simplified version matching paper's approach (no training phase)
//

#include "ClusterHead.h"
#include <cmath>
#include <algorithm>

Define_Module(ClusterHead);

void ClusterHead::loadCHData()
{
    if (dataLoaded) return;

    chData = new IntelLabData();

    std::string dataFile = par("dataFile").stringValue();
    if (dataFile.empty()) {
        dataFile = "../data.txt";
    }

    std::vector<int> moteIds = {1};
    std::string startDate = "2004-03-11";
    std::string endDate = "2004-03-14";

    bool loaded = chData->loadData(dataFile, moteIds, startDate, endDate);

    if (loaded) {
        EV << "=== CH DATA LOADED (MoteID=1) ===\n";
        EV << "CH readings: " << chData->getReadingsCount(1) << "\n";
        // 1 outlier mỗi batch (batch này chỉ có 1 sensor CH nên batchSize nhỏ hơn)
        chData->injectOutliers(1, 2.5, 10);
        EV << "CH outliers injected: " << chData->getTotalOutliers() << "\n";
        EV << "=================================\n";
    }

    dataLoaded = true;
}

void ClusterHead::addCHReading()
{
    if (!dataLoaded || chData == nullptr) return;

    SensorReading reading = chData->getNextReading(chMoteId);

    SensorMsg *chMsg = new SensorMsg("CHData");
    chMsg->setSourceId(chMoteId);
    chMsg->setTemperature(reading.temperature);
    chMsg->setHumidity(reading.humidity);
    chMsg->setLight(reading.light);
    chMsg->setVoltage(reading.voltage);
    chMsg->setIsOutlier(reading.isOutlier);

    dataBuffer.push_back(chMsg);
}

void ClusterHead::initialize()
{
    clusterSize = par("clusterSize").intValue();
    if (clusterSize <= 0) clusterSize = 20;

    chMoteId = 1;
    dataLoaded = false;
    chData = nullptr;

    threshold = par("threshold").doubleValue();
    if (threshold <= 0) threshold = 3.338;

    std::string algName = par("algorithm").stringValue();
    if (algName == "OD") {
        algorithm = ALG_OD;
        threshold = par("odThreshold").doubleValue();
        if (threshold <= 0) threshold = 15.0;
    } else {
        algorithm = ALG_ODA_MD;
    }

    loadCHData();

    energy = EnergyModel(5.0);

    totalPacketsReceived = 0;
    totalOutliersDetected = 0;
    totalPacketsForwarded = 0;

    logInterval = par("logInterval").doubleValue();
    if (logInterval <= 0) logInterval = 100.0;

    logTimer = new cMessage("logTimer");
    scheduleAt(simTime() + logInterval, logTimer);

    EV << "ClusterHead initialized: algorithm="
       << (algorithm == ALG_ODA_MD ? "ODA-MD" : "OD")
       << ", threshold=" << threshold
       << ", clusterSize=" << clusterSize << "\n";
}

void ClusterHead::handleMessage(cMessage *msg)
{
    if (msg == logTimer) {
        metrics.logMetrics(simTime());

        EV << "[" << simTime() << "] DA: "
           << (metrics.getDetectionAccuracy() * 100) << "%"
           << ", FAR: " << (metrics.getFalseAlarmRate() * 100) << "%\n";

        scheduleAt(simTime() + logInterval, logTimer);
        return;
    }

    SensorMsg *sMsg = check_and_cast<SensorMsg *>(msg);
    totalPacketsReceived++;
    dataBuffer.push_back(sMsg);
    energy.receive(256);

    // Khi buffer đầy, chạy thuật toán
    if ((int)dataBuffer.size() >= clusterSize) {
        
        if (algorithm == ALG_ODA_MD) {
            runODAMD();
        } else {
            runOD();
        }

        dataBuffer.clear();
    }
}

// === ODA-MD Algorithm (theo đúng bài báo) ===
void ClusterHead::runODAMD()
{
    int n = dataBuffer.size();

    // Chuyển đổi sang vector
    std::vector<std::vector<double>> X(n, std::vector<double>(4));
    for (int i = 0; i < n; i++) {
        X[i][0] = dataBuffer[i]->getTemperature();
        X[i][1] = dataBuffer[i]->getHumidity();
        X[i][2] = dataBuffer[i]->getLight();
        X[i][3] = dataBuffer[i]->getVoltage();
    }

    // STEP 1: Tính Mean từ batch hiện tại
    std::vector<double> mu = calculateMean(X);
    
    // STEP 2: Tính Covariance từ batch
    std::vector<std::vector<double>> Sigma = calculateCovariance(X, mu);

    // STEP 3: Nghịch đảo Covariance
    std::vector<std::vector<double>> InvSigma(4, std::vector<double>(4));
    bool success = invertMatrix4x4(Sigma, InvSigma);

    if (!success) {
        EV << "Warning: Singular Matrix!\n";
        for (auto sMsg : dataBuffer) {
            metrics.recordDetection(sMsg->isOutlier(), false);
            send(sMsg, "out");
            totalPacketsForwarded++;
        }
        return;
    }

    energy.process(1000);

    // STEP 4: Tính MD cho tất cả samples
    std::vector<double> mdValues(n);
    double maxMD = 0, minMD = 1e9;
    int actualOutlierCount = 0;
    
    for (int i = 0; i < n; i++) {
        mdValues[i] = calculateMahalanobis(X[i], mu, InvSigma);
        if (mdValues[i] > maxMD) maxMD = mdValues[i];
        if (mdValues[i] < minMD) minMD = mdValues[i];
        if (dataBuffer[i]->isOutlier()) actualOutlierCount++;
    }

    // === LƯU THÔNG TIN TRƯỚC KHI XÓA ===
    std::vector<bool> isActualOutlier(n);
    std::vector<int> sourceIds(n);
    for (int i = 0; i < n; i++) {
        isActualOutlier[i] = dataBuffer[i]->isOutlier();
        sourceIds[i] = dataBuffer[i]->getSourceId();
    }

    // === LOG CHI TIẾT NẾU CÓ OUTLIER THỰC SỰ TRONG BATCH ===
    if (actualOutlierCount > 0) {
        EV << "\n=== BATCH ANALYSIS (có " << actualOutlierCount << " outlier thực) ===\n";
        EV << "Mean: T=" << mu[0] << " H=" << mu[1] << " L=" << mu[2] << " V=" << mu[3] << "\n";
        EV << "Variance (diagonal): T=" << Sigma[0][0] << " H=" << Sigma[1][1] 
           << " L=" << Sigma[2][2] << " V=" << Sigma[3][3] << "\n";
        EV << "MD range: [" << minMD << ", " << maxMD << "], threshold=" << threshold << "\n";
        
        // Log từng sample với label TP/TN/FP/FN
        for (int i = 0; i < n; i++) {
            bool actual = isActualOutlier[i];
            bool detected = (mdValues[i] >= threshold);
            
            EV << "  [" << i << "] Node" << sourceIds[i]
               << " T=" << X[i][0] << " MD=" << mdValues[i];
            
            if (actual && detected) EV << " [TP]";
            else if (actual && !detected) EV << " [FN-MISSED!]";
            else if (!actual && detected) EV << " [FP]";
            else EV << " [TN]";
            
            EV << "\n";
        }
        EV << "=== END BATCH ===\n\n";
    }

    // STEP 5: Phát hiện outliers và xử lý
    int detectedCount = 0;

    for (int i = 0; i < n; i++) {
        bool detectedAsOutlier = (mdValues[i] >= threshold);

        metrics.recordDetection(isActualOutlier[i], detectedAsOutlier);

        if (detectedAsOutlier) {
            delete dataBuffer[i];
            totalOutliersDetected++;
            detectedCount++;
        } else {
            send(dataBuffer[i], "out");
            totalPacketsForwarded++;
            energy.transmit(256, 30.0);
        }
    }

    if (detectedCount > 0) {
        EV << "Batch result: " << detectedCount << "/" << n << " outliers detected.\n";
    }
}

void ClusterHead::runOD()
{
    int n = dataBuffer.size();

    std::vector<std::vector<double>> X(n, std::vector<double>(4));
    for (int i = 0; i < n; i++) {
        X[i][0] = dataBuffer[i]->getTemperature();
        X[i][1] = dataBuffer[i]->getHumidity();
        X[i][2] = dataBuffer[i]->getLight();
        X[i][3] = dataBuffer[i]->getVoltage();
    }

    std::vector<double> mu = calculateMean(X);
    energy.process(100);

    for (int i = 0; i < n; i++) {
        double ed = calculateEuclidean(X[i], mu);
        bool actualOutlier = dataBuffer[i]->isOutlier();
        bool detectedAsOutlier = (ed >= threshold);

        metrics.recordDetection(actualOutlier, detectedAsOutlier);

        if (detectedAsOutlier) {
            EV << " -> OD Outlier: Node " << dataBuffer[i]->getSourceId()
               << " ED=" << ed << "\n";
            delete dataBuffer[i];
            totalOutliersDetected++;
        } else {
            send(dataBuffer[i], "out");
            totalPacketsForwarded++;
            energy.transmit(256, 30.0);
        }
    }
}

// --- MATH FUNCTIONS ---

std::vector<double> ClusterHead::calculateMean(const std::vector<std::vector<double>>& data) {
    int n = data.size();
    std::vector<double> mean(4, 0.0);
    for (const auto& row : data) {
        for (int j = 0; j < 4; j++) mean[j] += row[j];
    }
    for (int j = 0; j < 4; j++) mean[j] /= n;
    return mean;
}

std::vector<std::vector<double>> ClusterHead::calculateCovariance(const std::vector<std::vector<double>>& data, const std::vector<double>& mean) {
    int n = data.size();
    std::vector<std::vector<double>> cov(4, std::vector<double>(4, 0.0));
    for (const auto& row : data) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                cov[j][k] += (row[j] - mean[j]) * (row[k] - mean[k]);
            }
        }
    }
    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
            cov[j][k] /= (n - 1);
        }
    }

    // Regularization để tránh singular matrix
    for (int j = 0; j < 4; j++) {
        cov[j][j] += 0.001;
    }

    return cov;
}

bool ClusterHead::invertMatrix4x4(const std::vector<std::vector<double>>& matrix, std::vector<std::vector<double>>& inverse) {
    int n = 4;
    std::vector<std::vector<double>> aug = matrix;

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            inverse[i][j] = (i == j) ? 1.0 : 0.0;

    for (int i = 0; i < n; i++) {
        int maxRow = i;
        for (int k = i + 1; k < n; k++) {
            if (std::abs(aug[k][i]) > std::abs(aug[maxRow][i]))
                maxRow = k;
        }
        std::swap(aug[i], aug[maxRow]);
        std::swap(inverse[i], inverse[maxRow]);

        double pivot = aug[i][i];
        if (std::abs(pivot) < 1e-9) return false;

        for (int j = 0; j < n; j++) {
            aug[i][j] /= pivot;
            inverse[i][j] /= pivot;
        }

        for (int k = 0; k < n; k++) {
            if (k != i) {
                double factor = aug[k][i];
                for (int j = 0; j < n; j++) {
                    aug[k][j] -= factor * aug[i][j];
                    inverse[k][j] -= factor * inverse[i][j];
                }
            }
        }
    }
    return true;
}

double ClusterHead::calculateMahalanobis(const std::vector<double>& sample, const std::vector<double>& mean, const std::vector<std::vector<double>>& invCov) {
    std::vector<double> diff(4);
    for (int i = 0; i < 4; i++) diff[i] = sample[i] - mean[i];

    double mdSq = 0.0;
    for (int i = 0; i < 4; i++) {
        double temp = 0.0;
        for (int j = 0; j < 4; j++) {
            temp += diff[j] * invCov[j][i];
        }
        mdSq += temp * diff[i];
    }
    return (mdSq > 0) ? std::sqrt(mdSq) : 0.0;
}

double ClusterHead::calculateEuclidean(const std::vector<double>& sample, const std::vector<double>& mean) {
    double sumSq = 0.0;
    for (int i = 0; i < 4; i++) {
        double diff = sample[i] - mean[i];
        sumSq += diff * diff;
    }
    return std::sqrt(sumSq);
}

void ClusterHead::finish()
{
    cancelAndDelete(logTimer);

    EV << "\n========================================\n";
    EV << "     CLUSTER HEAD FINAL REPORT\n";
    EV << "========================================\n";
    EV << "Algorithm: " << (algorithm == ALG_ODA_MD ? "ODA-MD" : "OD") << "\n";
    EV << "Threshold: " << threshold << "\n";
    EV << "Batch Size: " << clusterSize << "\n";
    EV << "----------------------------------------\n";
    EV << "Total Received:    " << totalPacketsReceived << "\n";
    EV << "Outliers Detected: " << totalOutliersDetected << "\n";
    EV << "Packets Forwarded: " << totalPacketsForwarded << "\n";
    EV << "Energy Consumed:   " << energy.getConsumedEnergyMJ() << " mJ\n";
    EV << "----------------------------------------\n";

    metrics.printSummary();

    std::string csvFile = (algorithm == ALG_ODA_MD) ? "metrics_odamd.csv" : "metrics_od.csv";
    metrics.exportToCSV(csvFile);

    if (chData != nullptr) {
        delete chData;
        chData = nullptr;
    }
}
