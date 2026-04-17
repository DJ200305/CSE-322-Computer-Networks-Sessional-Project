# CSE-322 Computer Networks Sessional Project

## FQRTT: Fuzzy-Adaptive RTT Estimation for Wireless Networks

### What This Is
Reproduction of QRTT ([Islam & Raghunathan, 2012](https://doi.org/10.1109/LES.2012.2229961)) with a proposed modification.
QRTT replaces the classical Jacobson RTT estimator with Q-learning, 
modeling transmission state (success/failure). We identified that its 
learning rate (α) and discount factor (γ) were hardcoded per topology, 
causing excess retransmissions and energy use in dynamic wireless environments.

Our fix: Fuzzy Logic to adaptively tune α and γ from live network signals
(RTT variance, loss rate, ACK delay trend) — no manual tuning required.

### Results (802.15.4 Wireless, base paper topology)
| Metric            | Jacobson | QRTT  | FQRTT (ours) |
|-------------------|----------|-------|--------------|
| Throughput (kbps) | 81.93    | 85.53 | 88.78        |
| Delivery Ratio    | 0.7355   | 0.7486| 0.7622       |
| Drop Ratio        | 0.2571   | 0.2465| 0.2219       |
| Total Energy (J)  | 377.97   | 408.16| 394.28       |

Note: In wired topologies, Jacobson still outperforms on delay and drop ratio —
expected, since wired losses are congestion-based and the two-state Q-model 
adds noise rather than signal in that regime.

### Simulated Across 15+ Configurations
Varied: nodes (20–100), flows (10–50), packets/sec (100–500), speed (5–25 m/s)

Topologies: Wired, 802.15.4 mobile wireless

Simulator: ns-3

## Repository Description

This repository contains only the files relevant to the QRTT work and it's modified version FQRTT from an `ns-3.45` tree.

Included files:

- `scratch/project-simulation.cc`
- `scratch/reportSimulation.cc`
- `scratch/reportSimulation_wireless.cc`
- `scratch/submission-wired.h`
- `scratch/submission-lrwpan-mobile.h`
- `scratch/submission-common.h`
- `src/internet/model/tcp-qlearning.cc`
- `src/internet/model/tcp-qlearning.h`
- `src/internet/model/tcp-socket-base.cc`

## Base Version

These files are intended to be applied to a clean `ns-3.45` source tree.

## How To Use

Copy the files into the matching locations inside a clean `ns-3.45` checkout:

```bash
cp scratch/project-simulation.cc /path/to/ns-3.45/scratch/
cp scratch/reportSimulation.cc /path/to/ns-3.45/scratch/
cp scratch/reportSimulation_wired.cc /path/to/ns-3.45/scratch/
cp scratch/submission-wired.h /path/to/ns-3.45/scratch/
cp scratch/submission-lrwpan-mobile.h /path/to/ns-3.45/scratch/
cp scratch/submission-common.h /path/to/ns-3.45/scratch/
cp src/internet/model/tcp-qlearning.cc /path/to/ns-3.45/src/internet/model/
cp src/internet/model/tcp-qlearning.h /path/to/ns-3.45/src/internet/model/
cp src/internet/model/tcp-socket-base.cc /path/to/ns-3.45/src/internet/model/
```

There are three types of files in scratch folder:
- project-simulation.cc
- reportSimulation.cc
- reportSimulation_wireless.cc

project-simulation.cc was build on topology 1 of base paper.
reportSimulation.cc was build on topologies and parameters given by project evaluators.

## Commands for running project-simulation.cc file
Build and run from the clean `ns-3.45` root:
If you want to use FuzzyQRTT (FQRTT), then use following command:
```bash
./ns3 build
./ns3 run "scratch/project-simulation.cc --useFuzzyQrtt=true"
```

If you want to use QRTT, then use following command:
```bash
./ns3 build
./ns3 run "scratch/project-simulation.cc --useQrtt=true"
```
If you want to use Jacobson, then use following command
```bash
./ns3 build
./ns3 run "scratch/project-simulation.cc"
```
## Commands for running the reportSimulation.cc files
Command style is same as project-simulation.cc. Just change the name.
