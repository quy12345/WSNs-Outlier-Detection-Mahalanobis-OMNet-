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
        // Inject exactly 1000 STRONG outliers as per paper
        chData->injectExactOutliers(1000, 5.0);  // multiplier=5.0 for strong outliers
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

    slidingWindow.push_back(chMsg);
}

void ClusterHead::initialize()
{
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
        
        // OD Algorithm: Fixed-width clustering parameter
        clusterWidth = par("clusterWidth").doubleValue();
        if (clusterWidth <= 0) clusterWidth = 50.0;  // Default cluster width
    } else {
        algorithm = ALG_ODA_MD;
    }

    loadCHData();

    energy = EnergyModel(5.0);

    totalPacketsReceived = 0;
    totalOutliersDetected = 0;
    totalPacketsForwarded = 0;
    isInitialWindowProcessed = false;  // First 20 samples not yet processed

    logInterval = par("logInterval").doubleValue();
    if (logInterval <= 0) logInterval = 100.0;

    logTimer = new cMessage("logTimer");
    scheduleAt(simTime() + logInterval, logTimer);

    // Request-Response pattern (Algorithm 1 - ODA-MD paper)
    requestInterval = par("requestInterval").doubleValue();
    if (requestInterval <= 0) requestInterval = 1.0;
    numSensors = gateSize("toSensor");
    requestId = 0;
    
    requestTimer = new cMessage("requestTimer");
    scheduleAt(simTime() + 0.1, requestTimer);  // First request after 0.1s

    EV << "ClusterHead initialized: algorithm="
       << (algorithm == ALG_ODA_MD ? "ODA-MD" : "OD")
       << ", threshold=" << threshold
       << ", windowSize=" << WINDOW_SIZE
       << ", numSensors=" << numSensors << "\n";
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
    
    // Handle request timer - send requests to all sensors (Algorithm 1)
    if (msg == requestTimer) {
        sendDataRequest();
        scheduleAt(simTime() + requestInterval, requestTimer);
        return;
    }

    SensorMsg *sMsg = check_and_cast<SensorMsg *>(msg);
    totalPacketsReceived++;
    energy.receive(256);

    // =========================================================================
    // SLIDING WINDOW MECHANISM (Real-time processing)
    // - Push new sample to the end of the window
    // - If window exceeds WINDOW_SIZE, remove oldest sample (pop_front)
    // - Process immediately when window is full (WINDOW_SIZE samples)
    // =========================================================================
    
    slidingWindow.push_back(sMsg);
    
    // Remove oldest sample if window exceeds size
    if ((int)slidingWindow.size() > WINDOW_SIZE) {
        // Delete the oldest message to prevent memory leak
        delete slidingWindow.front();
        slidingWindow.pop_front();
    }
    
    // Process immediately when window has exactly WINDOW_SIZE samples
    if ((int)slidingWindow.size() == WINDOW_SIZE) {
        if (algorithm == ALG_ODA_MD) {
            runODAMD();  // Real-time: process the newest sample immediately
        } else {
            runOD();     // OD still uses batch processing
        }
    }
}

// =============================================================================
// ODA-MD Algorithm with SLIDING WINDOW (Real-time processing)
// HYBRID APPROACH:
// - Initial 20 samples: Calculate MD for ALL samples, block outliers but keep in window
// - After initial 20: Calculate MD for NEWEST sample only, block/forward accordingly
// - All samples stay in window for error/event classification
// =============================================================================
void ClusterHead::runODAMD()
{
    int n = slidingWindow.size();
    if (n < WINDOW_SIZE) return;  // Wait until window is full

    // Convert sliding window to data matrix
    std::vector<std::vector<double>> X(n, std::vector<double>(4));
    for (int i = 0; i < n; i++) {
        X[i][0] = slidingWindow[i]->getTemperature();
        X[i][1] = slidingWindow[i]->getHumidity();
        X[i][2] = slidingWindow[i]->getLight();
        X[i][3] = slidingWindow[i]->getVoltage();
    }

    // STEP 1: Calculate Mean from current window (slides with new data)
    std::vector<double> mu = calculateMean(X);
    
    // STEP 2: Calculate Covariance from current window
    std::vector<std::vector<double>> Sigma = calculateCovariance(X, mu);

    // STEP 3: Invert Covariance matrix
    std::vector<std::vector<double>> InvSigma(4, std::vector<double>(4));
    bool success = invertMatrix4x4(Sigma, InvSigma);

    if (!success) {
        EV << "Warning: Singular Matrix!\n";
        // For newest sample only - forward without detection
        SensorMsg* newestMsg = slidingWindow.back();
        metrics.recordDetection(newestMsg->isOutlier(), false);
        send(newestMsg->dup(), "out");  // Send a copy (original stays in window)
        totalPacketsForwarded++;
        return;
    }

    // Energy consumption for matrix computation
    energy.process(1000);  // ~1000 FLOPs for 4x4 matrix inversion

    // =========================================================================
    // HYBRID DETECTION LOGIC
    // =========================================================================
    
    if (!isInitialWindowProcessed) {
        // =================================================================
        // INITIAL WINDOW: Calculate MD for ALL 20 samples
        // Outliers are blocked but STAY in window (for error/event detection)
        // =================================================================
        EV << "\n=== INITIAL WINDOW PROCESSING (all " << n << " samples) ===\n";
        EV << "Mean: T=" << mu[0] << " H=" << mu[1] << " L=" << mu[2] << " V=" << mu[3] << "\n";
        
        int detectedCount = 0;
        for (int i = 0; i < n; i++) {
            SensorMsg* msg = slidingWindow[i];
            double md = calculateMahalanobis(X[i], mu, InvSigma);
            bool actualOutlier = msg->isOutlier();
            bool detectedAsOutlier = (md >= threshold);
            int sourceId = msg->getSourceId();
            
            // Record detection metrics
            metrics.recordDetection(actualOutlier, detectedAsOutlier);
            
            // Log result
            EV << "  [" << i << "] Node" << sourceId
               << " T=" << X[i][0] << " MD=" << md;
            
            if (actualOutlier && detectedAsOutlier) {
                EV << " [TP]";
            } else if (actualOutlier && !detectedAsOutlier) {
                EV << " [FN-MISSED!]";
            } else if (!actualOutlier && detectedAsOutlier) {
                EV << " [FP]";
            } else {
                EV << " [TN]";
            }
            
            if (detectedAsOutlier) {
                EV << " -> BLOCKED";
                totalOutliersDetected++;
                detectedCount++;
                // Sample stays in window (not deleted) for error/event classification
            } else {
                EV << " -> FORWARDED";
                send(msg->dup(), "out");  // Send copy, original stays in window
                totalPacketsForwarded++;
                energy.transmit(256, 30.0);
            }
            EV << "\n";
        }
        
        EV << "=== INITIAL WINDOW DONE: " << detectedCount << "/" << n << " outliers blocked ===\n\n";
        isInitialWindowProcessed = true;
        
    } else {
        // =================================================================
        // SLIDING MODE: Only calculate MD for the NEWEST sample
        // =================================================================
        int newestIdx = n - 1;
        SensorMsg* newestMsg = slidingWindow[newestIdx];
        
        double md = calculateMahalanobis(X[newestIdx], mu, InvSigma);
        bool actualOutlier = newestMsg->isOutlier();
        bool detectedAsOutlier = (md >= threshold);
        int sourceId = newestMsg->getSourceId();
        
        // Record detection metrics
        metrics.recordDetection(actualOutlier, detectedAsOutlier);
        
        // Log detection result
        EV << "[SLIDING] Node" << sourceId
           << " T=" << X[newestIdx][0]
           << " MD=" << md;
        
        if (actualOutlier && detectedAsOutlier) {
            EV << " [TP]";
        } else if (actualOutlier && !detectedAsOutlier) {
            EV << " [FN-MISSED!]";
        } else if (!actualOutlier && detectedAsOutlier) {
            EV << " [FP]";
        } else {
            EV << " [TN]";
        }
        
        if (detectedAsOutlier) {
            EV << " -> BLOCKED\n";
            totalOutliersDetected++;
            // Sample stays in window for error/event classification
        } else {
            EV << " -> FORWARDED\n";
            send(newestMsg->dup(), "out");  // Send copy, original stays in window
            totalPacketsForwarded++;
            energy.transmit(256, 30.0);
        }
    }
}

// =============================================================================
// OD ALGORITHM (Fawzy et al., 2013) - Full 4-Step Implementation
// Paper: "Outliers detection and classification in wireless sensor networks"
// =============================================================================

void ClusterHead::runOD()
{
    int n = slidingWindow.size();

    // Convert buffer to data matrix
    std::vector<std::vector<double>> X(n, std::vector<double>(4));
    for (int i = 0; i < n; i++) {
        X[i][0] = slidingWindow[i]->getTemperature();
        X[i][1] = slidingWindow[i]->getHumidity();
        X[i][2] = slidingWindow[i]->getLight();
        X[i][3] = slidingWindow[i]->getVoltage();
        
        // Track sensor readings for trust calculation
        int sensorId = slidingWindow[i]->getSourceId();
        sensorTotalCount[sensorId]++;
    }

    energy.process(200);  // More computation than ODA-MD due to clustering

    // === STEP 1: Fixed-Width Clustering ===
    runOD_Clustering(X);
    
    // === STEP 2: Outlier Detection (Inter-cluster distance) ===
    runOD_Detection();
    
    // === STEP 3 & 4: Classification and Processing ===
    runOD_Classification(X);
    
    // OD uses batch processing - clear window after processing
    for (auto msg : slidingWindow) {
        delete msg;
    }
    slidingWindow.clear();
}

// -----------------------------------------------------------------------------
// STEP 1: Fixed-Width Clustering
// Assign points to clusters based on distance to cluster center
// -----------------------------------------------------------------------------
void ClusterHead::runOD_Clustering(const std::vector<std::vector<double>>& X)
{
    odClusters.clear();
    
    // ==========================================================================
    // OD ENERGY OVERHEAD: Clustering requires neighbor information exchange
    // Paper (Fawzy et al.): "clustering algorithm is applied to group data"
    // Each sensor broadcasts its data to neighbors to find cluster membership
    // ==========================================================================
    int numDataPoints = X.size();
    // Broadcast: each point sends 128 bits (4 attributes × 32 bits) to neighbors
    // Average distance to neighbor: 30m
    energy.transmit(128 * numDataPoints, 30.0);
    // Receive cluster assignments from potential cluster centers
    energy.receive(64 * numDataPoints);
    
    for (size_t i = 0; i < X.size(); i++) {
        bool assigned = false;
        
        // Try to assign to existing cluster
        for (auto& cluster : odClusters) {
            double dist = calculateEuclidean(X[i], cluster.center);
            if (dist <= clusterWidth) {
                // Assign to this cluster
                cluster.members.push_back(i);
                
                // Update cluster center (incremental mean)
                int m = cluster.members.size();
                for (int j = 0; j < 4; j++) {
                    cluster.center[j] = ((m - 1) * cluster.center[j] + X[i][j]) / m;
                }
                assigned = true;
                break;
            }
        }
        
        // Create new cluster if not assigned
        if (!assigned) {
            DataCluster newCluster;
            newCluster.center = X[i];
            newCluster.members.push_back(i);
            newCluster.isOutlier = false;
            newCluster.avgInterClusterDist = 0;
            odClusters.push_back(newCluster);
        }
    }
    
    EV << "[OD] Created " << odClusters.size() << " clusters from " << X.size() << " points\n";
}

// -----------------------------------------------------------------------------
// STEP 2: Outlier Detection
// A cluster is outlier if its avg inter-cluster distance > mean + std
// -----------------------------------------------------------------------------
void ClusterHead::runOD_Detection()
{
    int numClusters = odClusters.size();
    if (numClusters <= 1) return;
    
    // ==========================================================================
    // OD ENERGY OVERHEAD: Inter-cluster distance requires CH-to-CH communication
    // Paper (Fawzy et al.): "for each cluster, an algorithm of outlier detection
    // is launched to classify normal and outlier cluster"
    // Each cluster broadcasts its center to all other clusters
    // ==========================================================================
    // Each cluster sends center (128 bits) to all other clusters
    // Distance between CHs: ~50m (larger than sensor-to-CH)
    energy.transmit(128 * numClusters, 50.0);
    // Receive center info from all other clusters
    energy.receive(128 * numClusters * (numClusters - 1));
    
    // Calculate inter-cluster distances
    for (int i = 0; i < numClusters; i++) {
        double sumDist = 0;
        for (int j = 0; j < numClusters; j++) {
            if (i != j) {
                sumDist += calculateEuclidean(odClusters[i].center, odClusters[j].center);
            }
        }
        odClusters[i].avgInterClusterDist = sumDist / (numClusters - 1);
    }
    
    // Calculate mean and std of distances
    double sumDist = 0;
    for (const auto& cluster : odClusters) {
        sumDist += cluster.avgInterClusterDist;
    }
    double meanDist = sumDist / numClusters;
    
    double variance = 0;
    for (const auto& cluster : odClusters) {
        double diff = cluster.avgInterClusterDist - meanDist;
        variance += diff * diff;
    }
    double stdDist = std::sqrt(variance / numClusters);
    
    // Label outlier clusters (distance > mean + 1*std)
    double outlierThreshold = meanDist + stdDist;
    int outlierClusterCount = 0;
    
    for (auto& cluster : odClusters) {
        if (cluster.avgInterClusterDist > outlierThreshold) {
            cluster.isOutlier = true;
            outlierClusterCount++;
        }
    }
    
    EV << "[OD] Threshold=" << outlierThreshold << " (mean=" << meanDist 
       << " + std=" << stdDist << "), " << outlierClusterCount << " outlier clusters\n";
}

// -----------------------------------------------------------------------------
// STEP 3 & 4: Classification (Error vs Event) + Trust + Processing
// -----------------------------------------------------------------------------
void ClusterHead::runOD_Classification(const std::vector<std::vector<double>>& X)
{
    int detectedCount = 0;
    int bufferSize = slidingWindow.size();
    
    // ==========================================================================
    // OD ENERGY OVERHEAD: Classification requires additional message exchange
    // Paper (Fawzy et al.): "outlier classification is executed to separate 
    // error and event data" - requires checking if multiple sensors report same
    // ==========================================================================
    // Classification overhead: query neighbors to distinguish error vs event
    // 64 bits query per outlier cluster, 30m distance
    int numOutlierClusters = 0;
    for (const auto& cluster : odClusters) {
        if (cluster.isOutlier) numOutlierClusters++;
    }
    if (numOutlierClusters > 0) {
        energy.transmit(64 * numOutlierClusters * numSensors, 30.0);
        energy.receive(64 * numOutlierClusters * numSensors);
    }
    
    // First pass: collect all cluster sensor info (before any deletion)
    std::vector<std::set<int>> clusterSensors(odClusters.size());
    for (size_t c = 0; c < odClusters.size(); c++) {
        for (int idx : odClusters[c].members) {
            clusterSensors[c].insert(slidingWindow[idx]->getSourceId());
        }
    }
    
    // Track which messages to delete (don't delete in loop to avoid issues)
    std::vector<bool> toDelete(bufferSize, false);
    std::vector<bool> toSend(bufferSize, false);
    
    // Second pass: mark for deletion/sending
    for (size_t c = 0; c < odClusters.size(); c++) {
        const auto& cluster = odClusters[c];
        bool isEvent = (clusterSensors[c].size() >= 2);
        
        for (int idx : cluster.members) {
            bool actualOutlier = slidingWindow[idx]->isOutlier();
            bool detectedAsOutlier = cluster.isOutlier;
            int sensorId = slidingWindow[idx]->getSourceId();
            
            // Record metrics
            metrics.recordDetection(actualOutlier, detectedAsOutlier);
            
            if (detectedAsOutlier) {
                if (!isEvent) {
                    sensorErrorCount[sensorId]++;
                }
                
                EV << " -> OD Outlier: Node " << sensorId 
                   << " Cluster=" << c
                   << " Type=" << (isEvent ? "EVENT" : "ERROR") << "\n";
                
                toDelete[idx] = true;
                totalOutliersDetected++;
                detectedCount++;
            } else {
                toSend[idx] = true;
            }
        }
    }
    
    // Third pass: actually send in order (deletion handled in runOD)
    for (int i = 0; i < bufferSize; i++) {
        if (toSend[i]) {
            send(slidingWindow[i]->dup(), "out");  // Send copy for OD batch mode
            totalPacketsForwarded++;
            energy.transmit(256, 30.0);
        }
    }
    
    if (detectedCount > 0) {
        EV << "[OD] Batch result: " << detectedCount << "/" << bufferSize 
           << " outliers detected.\n";
    }
}

// -----------------------------------------------------------------------------
// STEP 4: Get Sensor Trust (called in finish())
// Trust = 1 - (errors / total)
// -----------------------------------------------------------------------------
double ClusterHead::getSensorTrust(int sensorId)
{
    if (sensorTotalCount.find(sensorId) == sensorTotalCount.end()) return 1.0;
    int total = sensorTotalCount[sensorId];
    int errors = sensorErrorCount[sensorId];
    if (total == 0) return 1.0;
    return 1.0 - ((double)errors / total);
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

// =============================================================================
// REQUEST-RESPONSE PATTERN (Algorithm 1 - ODA-MD Paper)
// "The CH_i sends a request req at t time, to all sensors belong to its cluster.
//  When a sensor N_k receives the request req, it starts the sensing process..."
// =============================================================================
void ClusterHead::sendDataRequest()
{
    requestId++;
    
    EV << "[" << simTime() << "] CH sending request #" << requestId 
       << " to " << numSensors << " sensors\n";
    
    // Send request to all sensors in the cluster
    for (int i = 0; i < numSensors; i++) {
        RequestMsg *req = new RequestMsg("DataRequest");
        req->setRequestId(requestId);
        
        send(req, "toSensor", i);
        
        // Energy consumption for request transmission (small control packet ~8 bytes)
        energy.transmit(64, 20.0);  // 64 bits = 8 bytes, 20m distance
    }
}

void ClusterHead::finish()
{
    cancelAndDelete(logTimer);
    cancelAndDelete(requestTimer);
    
    // Clean up remaining messages in sliding window
    for (auto msg : slidingWindow) {
        delete msg;
    }
    slidingWindow.clear();

    EV << "\n========================================\n";
    EV << "     CLUSTER HEAD FINAL REPORT\n";
    EV << "========================================\n";
    EV << "Algorithm: " << (algorithm == ALG_ODA_MD ? "ODA-MD (Sliding Window)" : "OD (Batch)") << "\n";
    EV << "Threshold: " << threshold << "\n";
    EV << "Window Size: " << WINDOW_SIZE << "\n";
    EV << "----------------------------------------\n";
    EV << "Total Received:    " << totalPacketsReceived << "\n";
    EV << "Outliers Detected: " << totalOutliersDetected << "\n";
    EV << "Packets Forwarded: " << totalPacketsForwarded << "\n";
    EV << "Energy Consumed:   " << energy.getConsumedEnergyMJ() << " mJ\n";
    EV << "----------------------------------------\n";

    metrics.printSummary();

    // =========================================================================
    // OD ALGORITHM STEP 4: Measuring Sensor Trustfulness (Fawzy et al., 2013)
    // Paper: "Trust(s_i) = 1 - (N_ol / N_i)"
    // Where: N_ol = erroneous readings, N_i = total readings
    // This computation is done locally at CH, so minimal energy overhead
    // =========================================================================
    if (algorithm == ALG_OD && !sensorTotalCount.empty()) {
        EV << "\n========================================\n";
        EV << "  OD STEP 4: SENSOR TRUSTFULNESS\n";
        EV << "========================================\n";
        
        // Energy for trust computation (local processing only, no communication)
        energy.process(50 * sensorTotalCount.size());  // ~50 FLOPs per sensor
        
        for (const auto& pair : sensorTotalCount) {
            int sensorId = pair.first;
            double trust = getSensorTrust(sensorId);
            int total = pair.second;
            int errors = (sensorErrorCount.find(sensorId) != sensorErrorCount.end()) 
                         ? sensorErrorCount[sensorId] : 0;
            
            EV << "  Sensor " << sensorId << ": "
               << "Total=" << total 
               << ", Errors=" << errors 
               << ", Trust=" << (trust * 100) << "%";
            
            // Classify sensor reliability
            if (trust >= 0.95) {
                EV << " [RELIABLE]\n";
            } else if (trust >= 0.80) {
                EV << " [MODERATE]\n";
            } else {
                EV << " [UNRELIABLE - Consider replacement]\n";
            }
        }
        EV << "========================================\n";
    }

    std::string csvFile = (algorithm == ALG_ODA_MD) ? "metrics_odamd.csv" : "metrics_od.csv";
    metrics.exportToCSV(csvFile);

    if (chData != nullptr) {
        delete chData;
        chData = nullptr;
    }
}
