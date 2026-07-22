# SSDsim: An Educational SSD Simulator for AI Workload Analysis

## Overview

SSDsim is a lightweight SSD simulator developed in C++ to study Flash Translation Layer (FTL) behavior, garbage collection, transaction scheduling, and SSD performance under modern AI workloads.

Unlike traditional educational SSD simulators that execute NAND operations immediately, SSDsim introduces a transaction-based execution model to separate host requests from NAND operations. This architecture enables future research on scheduling policies and event-driven SSD simulation.

The simulator is designed as a research-oriented platform for evaluating SSD behavior under AI inference and training workloads.

---

## Features

* Page-level Flash Translation Layer (FTL)
* Page Mapping
* DRAM Write Buffer
* Garbage Collection (GC)
* Transaction Queue
* NAND Read / Program / Erase Transactions
* Channel-aware NAND Timing Model
* AI-oriented Workload Generator
* Performance Statistics Collection

---

## Architecture

```
Host
 │
 ▼
Workload Generator
 │
 ▼
SSD Controller
 │
 ├── DRAM Controller
 │      ├── Mapping Table
 │      ├── Write Buffer
 │      └── Transaction Queue
 │
 └── Flash Controller
        ├── NAND Model
        ├── Free Block Management
        ├── Garbage Collection
       
```
