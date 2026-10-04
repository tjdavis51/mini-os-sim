# Mini OS Simulator

A C operating system simulator with CPU scheduling (FCFS, SJF, SRTF, and round robin), simulated I/O, memory allocation/access, and event logging.

## Run

Requires a C99 compiler, Make, and POSIX threads (macOS or Linux).

```sh
git clone https://github.com/tjdavis51/mini-os-sim.git
cd mini-os-sim/sim04
make -f sim04_mf
./sim04 -rs config0.cnf
```

Run from `sim04/` so the sample metadata path resolves. The sample prints events and writes `logfile_1.lgf`.

Use `-dc` to display configuration, `-dm` to display metadata, and `-rs` to run the simulation. Flags can be combined:

```sh
./sim04 -dc -dm -rs config0.cnf
```

Edit `config0.cnf` to change scheduling, timing, memory, and logging settings. Edit `metadata5.mdf` to change the simulated workload.

```sh
make -f sim04_mf clean
```

Timer utility attribution is preserved in `SimTimer.c` and `SimTimer.h`.
