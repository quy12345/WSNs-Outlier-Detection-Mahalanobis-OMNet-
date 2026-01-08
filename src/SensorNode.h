//
// SensorNode - Reads real Intel Lab data and sends to Cluster Head
//

#ifndef __ODAMD_SENSORNODE_H_
#define __ODAMD_SENSORNODE_H_

#include <omnetpp.h>
#include "IntelLabData.h"
#include "EnergyModel.h"

using namespace omnetpp;

class SensorNode : public cSimpleModule
{
  private:
    int nodeId;
    int realMoteId;          // Mote ID từ Intel Lab (36, 37, 38)
    cMessage *sendTimer;

    // Data source
    static IntelLabData* sharedData;  // Shared data source
    static bool dataLoaded;

    // Energy tracking
    EnergyModel energy;

    // Configuration
    double sendInterval;
    bool useRealData;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void loadSharedData();
};

#endif
