//
// Wireless Sensor Network for ODA-MD Simulation
// Based on paper: "Outlier Detection Algorithm based on Mahalanobis Distance for WSN"
// Titouna et al., ICCCI-2019
//
package oda_md.simulations;

import oda_md.SensorNode;
import oda_md.ClusterHead;
import oda_md.Sink;

//
// Simple Cluster 2 Network
// Contains: 1 Sink, 1 Cluster Head (CH2), 3 Sensors (representing nodes 36, 37, 38 from Intel Lab)
//
network ODA_MD_Network
{
    parameters:
        int numNodes = default(3);  // 3 sensors representing Intel Lab nodes 36, 37, 38
        @display("bgb=700,450;bgg=50,2,grey90;bgi=background/terrain,s");
    submodules:
        sink: Sink {
            @display("p=600,225;i=device/server2,gold,60;is=l");
        }
        clusterHead: ClusterHead {
            @display("p=350,225;i=device/accesspoint,cyan,50;is=l");
        }
        sensor[numNodes]: SensorNode {
            @display("p=100,100,c,120;i=device/palm;is=s");
        }
    connections:
        // Cluster Head -> Sink
        clusterHead.out --> sink.in;

        // Bidirectional: CH <-> Sensors
        // CH sends requests via toSensor[], sensors respond via out->in[]
        for i=0..numNodes-1 {
            clusterHead.toSensor++ --> sensor[i].in;   // Request: CH -> Sensor
            sensor[i].out --> clusterHead.in++;        // Response: Sensor -> CH
        }
}
