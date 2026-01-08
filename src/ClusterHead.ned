//
// ClusterHead NED module definition
// CH is also a sensor (moteId=1 from Intel Lab)
//
package oda_md;

simple ClusterHead
{
    parameters:
        int clusterSize = default(4);           // 3 sensors + 1 CH = 4
        double threshold = default(3.338);      // Chi-square threshold for ODA-MD
        double odThreshold = default(15.0);     // Euclidean threshold for OD baseline
        double clusterWidth = default(50.0);    // OD: Fixed-width clustering parameter
        string algorithm = default("ODA-MD");   // "ODA-MD" or "OD"
        string dataFile = default("../data.txt"); // Data file for CH's own readings
        double logInterval @unit(s) = default(100s);
        @display("i=device/server");
    gates:
        input in[];
        output out;
}
