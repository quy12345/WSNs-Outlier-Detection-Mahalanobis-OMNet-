# ODA-MD: Outlier Detection Algorithm based on Mahalanobis Distance

OMNeT++ simulation of the ODA-MD algorithm for Wireless Sensor Networks.

> **Paper**: Titouna et al., *"Outlier Detection Algorithm based on Mahalanobis Distance for Wireless Sensor Networks"*, ICCCI-2019

## Quick Start

### 1. Download Intel Lab Data
```bash
# Download data.txt (~150MB) from Intel Berkeley Lab
curl -o data.txt http://db.csail.mit.edu/labdata/data.txt.gz
gunzip data.txt.gz
```
Or download manually: [Intel Lab Data](http://db.csail.mit.edu/labdata/labdata.html)

### 2. Build & Run
```bash
# In OMNeT++ IDE: Import project → Build → Run
# Or command line:
make
cd simulations
../src/ODA-MD -u Qtenv -c ODAMD
```

## Configurations

| Config | Description |
|--------|-------------|
| `ODAMD` | ODA-MD algorithm (Mahalanobis Distance) |
| `OD` | Baseline algorithm (Euclidean Distance) |
| `QuickTest` | Quick 100s test run |

## Project Structure
```
├── src/           # Source code (ClusterHead, SensorNode, Sink)
├── simulations/   # Network config (WSN.ned, omnetpp.ini)
├── documents/     # Original paper (doc.pdf, doc.tex)
└── data.txt       # Intel Lab dataset (download separately)
```

## Key Parameters
- **Threshold**: 3.338 (√χ²₄,₀.₉₇₅)
- **Batch size**: 20 samples
- **Data sensors**: Mote IDs 36, 37, 38
- **Time range**: 2004-03-11 → 2004-03-14

## License
Educational use. Based on IEEE ICCCI-2019 paper.
