//
// SensorNode NED module definition
//
package oda_md;

simple SensorNode
{
    parameters:
        int nodeId = default(index);
        double sendInterval @unit(s) = default(1s);
        bool useRealData = default(true);
        string dataFile = default("../data.txt");
        @display("i=misc/sensor2");
    gates:
        output out;
}
