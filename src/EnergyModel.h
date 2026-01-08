//
// Energy Model for WSN based on Heinzelman's model (IEEE 2002)
// Reference: [24] W. B. Heinzelman, A. P. Chandrakasan, and H. Balakrishnan
//

#ifndef __ODAMD_ENERGYMODEL_H_
#define __ODAMD_ENERGYMODEL_H_

#include <omnetpp.h>
#include <cmath>

using namespace omnetpp;

class EnergyModel {
  private:
    // Energy parameters (from Heinzelman's model)
    static constexpr double E_ELEC = 50e-9;      // 50 nJ/bit - Electronics energy
    static constexpr double E_AMP = 100e-12;     // 100 pJ/bit/m^2 - Amplifier energy
    static constexpr double E_DA = 5e-9;         // 5 nJ/bit - Data aggregation
    static constexpr double D0 = 87.0;           // Threshold distance (m)

    double currentEnergy;    // Current remaining energy (J)
    double consumedEnergy;   // Total consumed energy (J)
    double initialEnergy;    // Initial energy (J)

  public:
    EnergyModel(double initEnergy = 2.0) {  // Default 2 Joules
        initialEnergy = initEnergy;
        currentEnergy = initEnergy;
        consumedEnergy = 0.0;
    }

    // Transmit k bits over distance d
    double transmit(int bits, double distance) {
        double energy;
        if (distance < D0) {
            // Free space model
            energy = E_ELEC * bits + E_AMP * bits * distance * distance;
        } else {
            // Multipath fading model
            double d4 = distance * distance * distance * distance;
            energy = E_ELEC * bits + 0.0013e-12 * bits * d4;
        }
        consumeEnergy(energy);
        return energy;
    }

    // Receive k bits
    double receive(int bits) {
        double energy = E_ELEC * bits;
        consumeEnergy(energy);
        return energy;
    }

    // Data aggregation (processing)
    double aggregate(int bits) {
        double energy = E_DA * bits;
        consumeEnergy(energy);
        return energy;
    }

    // CPU processing (for ODA-MD algorithm)
    double process(int operations) {
        // Giả định: 10 nJ per operation for matrix calculations
        double energy = 10e-9 * operations;
        consumeEnergy(energy);
        return energy;
    }

    // Consume energy
    void consumeEnergy(double energy) {
        consumedEnergy += energy;
        currentEnergy -= energy;
        if (currentEnergy < 0) currentEnergy = 0;
    }

    // Check if node is alive
    bool isAlive() const {
        return currentEnergy > 0;
    }

    // Get remaining energy
    double getRemainingEnergy() const {
        return currentEnergy;
    }

    // Get consumed energy
    double getConsumedEnergy() const {
        return consumedEnergy;
    }

    // Get energy percentage
    double getEnergyPercentage() const {
        return (currentEnergy / initialEnergy) * 100.0;
    }

    // Reset energy
    void reset() {
        currentEnergy = initialEnergy;
        consumedEnergy = 0.0;
    }

    // Convert energy to milliJoules for display
    double getConsumedEnergyMJ() const {
        return consumedEnergy * 1000.0;
    }

    // =========================================================================
    // MICA2 Processing Delay (Paper: ATmega128L @ 8MHz, no FPU)
    // =========================================================================
    // Matrix 4x4 inversion: ~1000 FLOPs
    // MICA2 8MHz with software FP: ~100 cycles/FLOP
    // Total: 1000 * 100 / 8MHz = 12.5ms per batch
    // =========================================================================
    double getProcessingDelaySeconds(int operations) const {
        // MICA2: 8MHz clock, ~100 cycles per floating-point operation (no FPU)
        const double CYCLES_PER_FLOP = 100.0;
        const double CLOCK_FREQ = 8e6;  // 8 MHz
        double cycles = operations * CYCLES_PER_FLOP;
        return cycles / CLOCK_FREQ;
    }
};

#endif
