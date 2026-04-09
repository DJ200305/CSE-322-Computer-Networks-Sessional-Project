# CSE-322 Computer Networks Sessional Project

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
