//
// Intel Lab Data Reader for ODA-MD Simulation
// Reads real sensor data from Intel Berkeley Lab dataset
//

#ifndef __ODAMD_INTELLABDATA_H_
#define __ODAMD_INTELLABDATA_H_

#include <omnetpp.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

using namespace omnetpp;

struct SensorReading {
    std::string date;
    std::string time;
    int epoch;
    int moteId;
    double temperature;
    double humidity;
    double light;
    double voltage;
    bool isOutlier;  // Dùng để đánh dấu outlier giả lập
};

class IntelLabData {
  private:
    std::map<int, std::vector<SensorReading>> sensorData;  // moteId -> readings
    std::map<int, size_t> readIndex;  // Track current read position per sensor
    int totalReadings;
    int totalOutliers;

  public:
    IntelLabData() : totalReadings(0), totalOutliers(0) {}

    // Load data from file, filtering by mote IDs and date range
    bool loadData(const std::string& filename,
                  const std::vector<int>& moteIds,
                  const std::string& startDate,
                  const std::string& endDate) {

        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            SensorReading reading;

            // Parse: date time epoch moteId temperature humidity light voltage
            iss >> reading.date >> reading.time >> reading.epoch >> reading.moteId
                >> reading.temperature >> reading.humidity
                >> reading.light >> reading.voltage;

            if (iss.fail()) continue;  // Skip malformed lines

            // Check if moteId is in our filter list
            bool moteMatch = false;
            for (int id : moteIds) {
                if (reading.moteId == id) {
                    moteMatch = true;
                    break;
                }
            }
            if (!moteMatch) continue;

            // Check date range
            if (reading.date >= startDate && reading.date <= endDate) {
                reading.isOutlier = false;
                sensorData[reading.moteId].push_back(reading);
                totalReadings++;
            }
        }

        file.close();

        // Initialize read indices
        for (int id : moteIds) {
            readIndex[id] = 0;
        }

        return totalReadings > 0;
    }

    // =========================================================================
    // Inject outliers: ĐẢM BẢO CHÍNH XÁC 1-2 OUTLIERS MỖI BATCH
    // =========================================================================
    // Cách tiếp cận: Mô phỏng round-robin reading order giống simulation thực tế
    // Batch size = 20, 3 sensors đọc luân phiên → batch i chứa readings từ vị trí
    // tương ứng của mỗi sensor.
    //
    // Để có 1 outlier/batch: chỉ inject 1 reading trong mỗi khoảng 20 readings TỔNG
    // Với 3 sensors: mỗi sensor đóng góp ~7 readings/batch
    // → inject mỗi 7 readings của 1 sensor = ~1 outlier/batch
    // =========================================================================
    void injectOutliers(int outliersPerBatch = 1, double multiplier = 2.5, int batchSize = 20) {
        if (totalReadings == 0) return;

        std::srand(42);  // Fixed seed for reproducibility
        
        int numSensors = sensorData.size();
        if (numSensors == 0) return;
        
        // Số readings mỗi sensor đóng góp cho 1 batch
        int readingsPerSensorPerBatch = batchSize / numSensors;  // ~6-7 với 3 sensors
        if (readingsPerSensorPerBatch < 1) readingsPerSensorPerBatch = 1;
        
        // Để đảm bảo outliersPerBatch outliers mỗi batch:
        // Interval giữa các outlier trong MỖI sensor = readingsPerSensorPerBatch / outliersPerBatch
        // Nhưng để phân bố đều, ta chỉ inject vào 1 sensor mỗi batch, luân phiên
        
        // Tính số batch tổng cộng dựa trên sensor có ít readings nhất
        int minReadings = INT_MAX;
        for (const auto& pair : sensorData) {
            if ((int)pair.second.size() < minReadings) {
                minReadings = pair.second.size();
            }
        }
        int totalBatches = (minReadings * numSensors) / batchSize;
        
        // Với mỗi batch, inject đúng outliersPerBatch outliers
        // Chiến lược: Inject vào sensor luân phiên, tại vị trí đầu batch
        int currentBatch = 0;
        std::vector<int> sensorIds;
        for (const auto& pair : sensorData) {
            sensorIds.push_back(pair.first);
        }
        
        for (int batch = 0; batch < totalBatches && totalOutliers < totalBatches * outliersPerBatch; batch++) {
            // Vị trí reading trong mỗi sensor cho batch này
            int readingIdx = batch * readingsPerSensorPerBatch;
            
            // Inject vào sensor được chọn (luân phiên để phân bố đều)
            for (int o = 0; o < outliersPerBatch && o < numSensors; o++) {
                int sensorIdx = (batch + o) % numSensors;
                int moteId = sensorIds[sensorIdx];
                
                // Kiểm tra bounds
                if (readingIdx >= (int)sensorData[moteId].size()) continue;
                
                SensorReading& reading = sensorData[moteId][readingIdx];
                if (!reading.isOutlier) {
                    // === MULTIVARIATE OUTLIER ===
                    reading.temperature *= multiplier;      // T tăng
                    reading.humidity *= (1.0 / multiplier); // H giảm  
                    reading.light *= multiplier;            // L tăng
                    reading.voltage *= 1.2;                 // V tăng 20%
                    
                    reading.isOutlier = true;
                    totalOutliers++;
                }
            }
        }
    }
    
    // Overload để tương thích với code cũ (inject ~count outliers tổng cộng)
    void injectOutliersLegacy(int count, double multiplier = 2.5, int batchSize = 20) {
        if (totalReadings == 0) return;
        
        int numSensors = sensorData.size();
        if (numSensors == 0) return;
        
        // Tính số batch dựa trên tổng readings
        int totalBatches = totalReadings / batchSize;
        
        // Số outliers mỗi batch để đạt được count outliers tổng
        int outliersPerBatch = (count + totalBatches - 1) / totalBatches;
        if (outliersPerBatch < 1) outliersPerBatch = 1;
        if (outliersPerBatch > 2) outliersPerBatch = 2;  // Cap at 2
        
        injectOutliers(outliersPerBatch, multiplier, batchSize);
    }


    // Get next reading for a specific mote (circular)
    SensorReading getNextReading(int moteId) {
        if (sensorData.find(moteId) == sensorData.end() ||
            sensorData[moteId].empty()) {
            // Return default reading if no data
            SensorReading empty;
            empty.moteId = moteId;
            empty.temperature = 20.0;
            empty.humidity = 40.0;
            empty.light = 100.0;
            empty.voltage = 2.5;
            empty.isOutlier = false;
            return empty;
        }

        auto& readings = sensorData[moteId];
        size_t& idx = readIndex[moteId];

        SensorReading reading = readings[idx];
        idx = (idx + 1) % readings.size();  // Circular

        return reading;
    }

    // Get total readings count
    int getTotalReadings() const { return totalReadings; }

    // Get readings count for a specific mote
    int getReadingsCount(int moteId) const {
        auto it = sensorData.find(moteId);
        if (it != sensorData.end()) {
            return it->second.size();
        }
        return 0;
    }

    // Get total injected outliers
    int getTotalOutliers() const { return totalOutliers; }
};

#endif
