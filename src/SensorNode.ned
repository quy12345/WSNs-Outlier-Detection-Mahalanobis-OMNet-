//
// SensorNode NED module definition
//
package oda_md;

simple SensorNode
{
    parameters:
        int nodeId = default(index);
        bool useRealData = default(true);
        string dataFile = default("../data.txt");
        @display("i=device/palm;is=s;tt=Intel Lab Sensor Node");
    gates:
        input in;       // Receive request from CH
        output out;     // Send data response to CH
}
