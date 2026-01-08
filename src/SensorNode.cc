//
// SensorNode - Reads real Intel Lab data and sends to Cluster Head
// Request-Response pattern: responds to RequestMsg from CH (Algorithm 1 - ODA-MD paper)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//

#include "SensorNode.h"
#include "messages_m.h"

Define_Module(SensorNode);

// Static member initialization
IntelLabData* SensorNode::sharedData = nullptr;
bool SensorNode::dataLoaded = false;

void SensorNode::loadSharedData()
{
    if (dataLoaded) return;

    sharedData = new IntelLabData();

    // Get data file path from parameter or default
    std::string dataFile = par("dataFile").stringValue();
    if (dataFile.empty()) {
        dataFile = "../data.txt";
    }

    // Mote IDs to filter (nodes 36, 37, 38 theo bài báo)
    std::vector<int> moteIds = {36, 37, 38};

    // Date range from paper: 2004-03-11 to 2004-03-14
    std::string startDate = "2004-03-11";
    std::string endDate = "2004-03-14";

    bool loaded = sharedData->loadData(dataFile, moteIds, startDate, endDate);

    if (loaded) {
        EV << "=== INTEL LAB DATA LOADED ===\n";
        EV << "Total readings: " << sharedData->getTotalReadings() << "\n";
        EV << "Node 36: " << sharedData->getReadingsCount(36) << " readings\n";
        EV << "Node 37: " << sharedData->getReadingsCount(37) << " readings\n";
        EV << "Node 38: " << sharedData->getReadingsCount(38) << " readings\n";

        // Inject exactly 1000 STRONG outliers as per paper
        sharedData->injectExactOutliers(1000, 5.0);  // multiplier=5.0 for strong outliers
        EV << "Injected outliers: " << sharedData->getTotalOutliers() << "\n";
        EV << "=============================\n";
    } else {
        EV << "WARNING: Could not load Intel Lab data from " << dataFile << "\n";
        EV << "Will use synthetic data instead.\n";
    }

    dataLoaded = true;
}

void SensorNode::initialize()
{
    // Lấy ID từ tham số trong file .ned
    nodeId = par("nodeId");

    // Map node index to real mote ID
    // sensor[0] -> mote 36, sensor[1] -> mote 37, sensor[2] -> mote 38
    int index = getIndex();
    if (index == 0) realMoteId = 36;
    else if (index == 1) realMoteId = 37;
    else realMoteId = 38;

    // Configuration
    useRealData = par("useRealData").boolValue();

    // Load shared data (only first sensor does this)
    if (useRealData) {
        loadSharedData();
    }

    // Initialize energy (2J theo Heinzelman)
    energy = EnergyModel(2.0);

    EV << "SensorNode " << nodeId << " (MoteID=" << realMoteId << ") initialized.\n";
    EV << "  [Request-Response mode: waiting for requests from CH]\n";
}

// =============================================================================
// REQUEST-RESPONSE PATTERN (Algorithm 1 - ODA-MD Paper)
// "When a sensor N_k receives the request req, it starts the sensing process
//  and it will send all measured data to CH_i"
// =============================================================================
void SensorNode::handleMessage(cMessage *msg)
{
    // Check if this is a request from CH
    RequestMsg *req = dynamic_cast<RequestMsg *>(msg);
    
    if (req != nullptr) {
        // Received request from CH - start sensing and respond
        
        // Check if still have energy
        if (!energy.isAlive()) {
            EV << "SensorNode " << nodeId << " is out of energy!\n";
            delete msg;
            return;
        }

        // Energy consumption for receiving request
        energy.receive(64);  // 64 bits = 8 bytes control packet

        // 1. Tạo gói tin SensorMsg phản hồi
        SensorMsg *sMsg = new SensorMsg("SensorData");
        sMsg->setSourceId(realMoteId);  // Use real mote ID

        bool isOutlier = false;

        if (useRealData && sharedData != nullptr) {
            // 2a. Lấy dữ liệu thực từ Intel Lab
            SensorReading reading = sharedData->getNextReading(realMoteId);

            sMsg->setTemperature(reading.temperature);
            sMsg->setHumidity(reading.humidity);
            sMsg->setLight(reading.light);
            sMsg->setVoltage(reading.voltage);
            isOutlier = reading.isOutlier;

        } else {
            // 2b. Dữ liệu giả lập (fallback)
            sMsg->setTemperature(normal(23, 2));
            sMsg->setHumidity(normal(35, 5));
            sMsg->setLight(normal(400, 100));
            sMsg->setVoltage(normal(2.5, 0.1));

            // 5% chance of outlier
            if (uniform(0, 1) < 0.05) {
                sMsg->setTemperature(normal(60, 10));  // Abnormal temp
                isOutlier = true;
            }
        }

        sMsg->setIsOutlier(isOutlier);

        if (isOutlier) {
            EV << "SensorNode " << realMoteId << " responding with OUTLIER data! "
               << "T=" << sMsg->getTemperature() << "\n";
        }

        // 3. Consume energy for transmission
        // Packet size: 32 bytes = 256 bits, distance ~20m to CH
        energy.transmit(256, 20.0);

        // 4. Gửi phản hồi sang Cluster Head
        send(sMsg, "out");

        // Delete the request message
        delete msg;
    }
    else {
        // Unknown message type
        EV << "SensorNode " << realMoteId << " received unknown message type\n";
        delete msg;
    }
}

void SensorNode::finish()
{
    EV << "SensorNode " << realMoteId << " Energy consumed: "
       << energy.getConsumedEnergyMJ() << " mJ ("
       << (100 - energy.getEnergyPercentage()) << "% used)\n";

    // Clean up shared data if this is the last sensor
    if (getIndex() == 2 && sharedData != nullptr) {
        delete sharedData;
        sharedData = nullptr;
        dataLoaded = false;
    }
}
