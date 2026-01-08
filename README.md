# ODA-MD: Outlier Detection Algorithm based on Mahalanobis Distance

OMNeT++ simulation of the ODA-MD algorithm for Wireless Sensor Networks.

> **Paper**: Titouna et al., *"Outlier Detection Algorithm based on Mahalanobis Distance for Wireless Sensor Networks"*, ICCCI-2019

## Results

| Metric | ODA-MD | Paper Target |
|--------|--------|--------------|
| Detection Accuracy (DA) | **99.79%** | ~95-100% |
| False Alarm Rate (FAR) | **0.07%** | ~1-5% |
| Precision | **98.78%** | - |

## Quick Start

### 1. Download Intel Lab Data

Download `data.txt` (~150MB) from [Intel Lab Data](http://db.csail.mit.edu/labdata/labdata.html)

### 2. Build & Run

```bash
# In OMNeT++ IDE: Import project → Build → Run
# Or use OMNeT++ command line environment
```

## Configurations

| Config | Description |
|--------|-------------|
| `ODAMD` | ODA-MD algorithm (Mahalanobis Distance) |
| `OD` | Baseline algorithm (Euclidean Distance) |
| `QuickTest` | Quick 100s test run |

## Key Parameters

| Parameter | Value | Source |
|-----------|-------|--------|
| Threshold | 3.338 | √χ²₄,₀.₉₇₅ |
| Batch size | 20 | Paper queue size |
| Outliers | 1000 | Paper specification |
| Sensors | 36, 37, 38 | Intel Lab nodes |
| Time range | 2004-03-11 → 2004-03-14 | Paper |

## Project Structure

```text
├── src/           # Source code (ClusterHead, SensorNode, Sink)
├── simulations/   # Network config (WSN.ned, omnetpp.ini)
└── data.txt       # Intel Lab dataset (download separately)
```

## License

Educational use. Based on IEEE ICCCI-2019 paper.
