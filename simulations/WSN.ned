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
        @display("bgb=600,400;bgl=2");
    submodules:
        sink: Sink {
            @display("p=500,200;i=device/server2");
        }
        clusterHead: ClusterHead {
            @display("p=300,200;i=device/server");
        }
        sensor[numNodes]: SensorNode {
            @display("p=100,100,c,100;i=misc/sensor2");
        }
    connections:
        // Cluster Head -> Sink
        clusterHead.out --> sink.in;

        // All Sensors -> Cluster Head
        for i=0..numNodes-1 {
            sensor[i].out --> clusterHead.in++;
        }
}
