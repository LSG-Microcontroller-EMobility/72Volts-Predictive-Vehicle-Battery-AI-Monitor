# Predictive Vehicle Battery Monitor

Embedded monitoring and predictive-diagnostics system for a **72 V vehicle battery pack composed of six monitored batteries**.

The project combines distributed voltage acquisition, SD logging, battery-balance checks, audio alerts, and an embedded neural-network model that compares the measured battery behavior with the expected one while the vehicle is operating.

## System Architecture

The system is composed of:

- **1 main controller** running `72Volts-Predictive-Vehicle-Battery-AI-Monitor.ino`
- **6 ATtiny85 measurement modules**, one for each battery
- an analog multiplexer used by the main controller to select each measurement channel
- an SD card for CSV logging
- a DFPlayer Mini for audio warnings
- current and power measurement inputs used by the predictive model

Simplified data flow:

```text
Battery B0 -> ATtiny85 0 --+
Battery B1 -> ATtiny85 1 --+
Battery B2 -> ATtiny85 2 --+
Battery B3 -> ATtiny85 3 --+--> Multiplexer --> Main Controller
Battery B4 -> ATtiny85 4 --+                       |
Battery B5 -> ATtiny85 5 --+                       +--> SD logging
                                                     +--> Balance check
                                                     +--> AI prediction
                                                     +--> Audio alarms
```

## ATtiny85 Measurement Modules

Each battery is monitored by a dedicated ATtiny85 running:

```text
ATtiny85-Analog-Battery-Voltage-Monitor.ino
```

Each module:

- reads the battery voltage on analog input `A2`
- uses an **external 3.90 V analog reference**
- performs **20 ADC readings** and averages them
- uses ADC prescaler 64
- refreshes the measurement when triggered through an external interrupt
- transmits the latest value using `SoftwareSerial` at **600 baud**

The transmitted frame has the form:

```text
<voltage><message ID>*
```

where `*` is used as the synchronization / frame delimiter.

The main controller triggers all six ATtiny85 nodes and then reads them one at a time through the multiplexer.

## Battery Monitoring

For every acquisition cycle, the main controller collects the six battery values:

```text
B0 B1 B2 B3 B4 B5
```

The firmware validates the received data and checks that the measurements belonging to the same acquisition cycle have a consistent message ID.

After all six values have been acquired, the system:

1. checks battery imbalance;
2. reads current and power data;
3. writes the measurements to the SD card;
4. periodically runs the predictive model.

## Battery Imbalance Detection

The firmware calculates the highest and lowest battery voltages and compares their percentage difference.

The allowed imbalance is dynamically adjusted according to the average battery voltage instead of using only a fixed threshold.

If the difference becomes excessive, the system generates an audio warning.

## Embedded Neural Network

The predictive model runs directly on the main microcontroller.

Network structure:

```text
2 inputs
25 hidden neurons
6 outputs
```

Inputs:

```text
Current
Power / energy-related value
```

Outputs:

```text
Expected voltage of batteries B0 ... B5
```

The hidden layer uses **Leaky ReLU** activation.

Weights and biases are stored in EEPROM and loaded during the forward pass, reducing SRAM usage.

The model is trained externally; the embedded firmware performs inference only.

## Predictive Diagnostics

The six predicted battery voltages are compared with the six measured values.

The firmware calculates:

```text
MSE -> RMSE -> normalized error percentage
```

The current maximum accepted prediction error is:

```text
20%
```

An instability warning is generated when the prediction error is too high and the vehicle current is greater than approximately:

```text
15 A
```

The idea is to detect batteries whose behavior deviates from the learned normal response under real vehicle load, even before a simple fixed voltage threshold is exceeded.

## SD Card Logging

Measurements are stored in CSV files:

```text
batt0.csv
batt1.csv
...
```

CSV format:

```text
IDMessage;Battery;Value;W/h;amps
```

Example:

```text
1;B0;12.54;;
1;B1;12.49;;
1;B2;12.51;;
;;;1250.35;18.42
```

## Audio Diagnostics

A DFPlayer Mini provides prerecorded warnings for events such as:

- battery imbalance
- invalid measurement
- SD card error
- incorrect acquisition ID
- system initialization
- data acquisition
- detected system instability

## Main Firmware Files

```text
72Volts-Predictive-Vehicle-Battery-AI-Monitor.ino
ATtiny85-Analog-Battery-Voltage-Monitor.ino
```

The complete system therefore uses **seven microcontrollers**: one central controller and six distributed ATtiny85 voltage-monitoring nodes.

## Project Status

This is an experimental embedded predictive-maintenance project developed for a real 72 V battery-monitoring architecture.

It combines:

- distributed sensing
- AVR microcontrollers
- embedded data logging
- battery diagnostics
- neural-network inference on a resource-constrained MCU

It should be considered a **prototype / R&D system**, not a replacement for a certified Battery Management System.

## Author

**Luigi Santagada**

Embedded systems, microcontrollers, battery monitoring and neural networks for resource-constrained devices.
