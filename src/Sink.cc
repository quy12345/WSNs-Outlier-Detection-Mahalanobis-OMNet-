//
// Sink - Receives clean data from ClusterHead and collects final statistics
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//

#include "Sink.h"
#include "messages_m.h"

Define_Module(Sink);

void Sink::initialize()
{
    totalReceived = 0;
    truePositives = 0;

    // Watch variables in GUI
    WATCH(totalReceived);
}

void Sink::handleMessage(cMessage *msg)
{
    SensorMsg *sMsg = check_and_cast<SensorMsg *>(msg);

    totalReceived++;

    // Log received data
    EV << "Sink received CLEAN data from Node " << sMsg->getSourceId()
       << " | T=" << sMsg->getTemperature()
       << " H=" << sMsg->getHumidity()
       << " L=" << sMsg->getLight()
       << " V=" << sMsg->getVoltage() << "\n";

    delete msg;
}

void Sink::finish()
{
    EV << "\n========================================\n";
    EV << "           SINK SUMMARY\n";
    EV << "========================================\n";
    EV << "Total Clean Packets Received: " << totalReceived << "\n";
    EV << "========================================\n";
}
