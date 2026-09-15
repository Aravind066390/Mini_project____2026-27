# High-Speed Zero-Copy Data Transfer System

## Project Objectives

1. **Develop a high-speed data transfer system** using **XDP, eBPF, AF_XDP, and io_uring** to minimize unnecessary data copies and reduce processing overhead during data transmission between the Internet, servers, and hosts.

2. **Implement two data-sharing modes:**

   * **Host-to-Host:** Enable direct data transfer between two participating hosts through the system.
   * **Host-to-Many:** Allow a host to publish data through the system so that multiple users or hosts can access the shared content.

3. **Design a server-mediated data transfer architecture** in which intermediate data is handled by a server while leveraging **zero-copy/low-copy data paths, shared buffers, and asynchronous I/O** to improve throughput, reduce latency, and maintain responsiveness under high data loads.

---

## System Modules and Responsibilities

| #     | Module                                   | Main Responsibility                                                                                                                                       | Technologies                                 |
| ----- | ---------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------- |
| **1** | **Network & Data Ingestion**             | Receive incoming packets, classify traffic, extract data chunks, and inject them into the application data path.                                          | XDP, eBPF, AF_XDP                            |
| **2** | **Buffer & Data-Path Manager**           | Manage circular/ring buffers, data chunking, buffer reuse, ownership, and movement of data between processing stages while minimizing unnecessary copies. | C/C++, `mmap`, Shared Memory, Ring Buffers   |
| **3** | **Storage & Asynchronous I/O**           | Handle data transfer to and from SSD/storage while supporting asynchronous file operations and efficient I/O paths.                                       | `io_uring`, Linux Filesystem, Direct I/O     |
| **4** | **Server & Data Distribution**           | Manage host-to-host and host-to-many communication, users, metadata, connections, and content distribution.                                               | C/C++, Java, TCP/UDP, HTTP, Sockets          |
| **5** | **Client & Data Reconstruction**         | Receive data chunks, reorder and reassemble them, and provide mechanisms for viewing, streaming, or downloading files, images, and video.                 | C/C++, Java, Networking, File/Video Handling |
| **6** | **Integration & Performance Evaluation** | Integrate all system modules and evaluate throughput, latency, CPU utilization, memory usage, data copies, and I/O performance.                           | Linux, `perf`, Benchmarking Tools            |

---

## High-Level Data Path

```text
                         ┌──────────────────────┐
                         │       INTERNET       │
                         └──────────┬───────────┘
                                    │
                                    ▼
                    ┌─────────────────────────────┐
                    │    NETWORK & INGESTION      │
                    │                             │
                    │     XDP / eBPF / AF_XDP     │
                    └──────────────┬──────────────┘
                                   │
                                   ▼
                    ┌─────────────────────────────┐
                    │    BUFFER & DATA-PATH       │
                    │          MANAGER            │
                    │                             │
                    │ Shared Memory / mmap        │
                    │ Ring Buffers / Buffer Pool  │
                    │ Ownership / Buffer Reuse    │
                    └──────────────┬──────────────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │                             │
                    ▼                             ▼
          ┌───────────────────┐        ┌────────────────────┐
          │   STORAGE & I/O   │        │ SERVER & DATA      │
          │                   │        │ DISTRIBUTION       │
          │     io_uring      │        │                    │
          │ Linux Filesystem  │        │ Host-to-Host       │
          │   Direct I/O      │        │ Host-to-Many       │
          └─────────┬─────────┘        └──────────┬─────────┘
                    │                             │
                    │                  ┌──────────┼──────────┐
                    │                  │          │          │
                    │                  ▼          ▼          ▼
                    │             ┌────────┐ ┌────────┐ ┌────────┐
                    │             │ Host A │ │ Host B │ │ Host N │
                    │             └────────┘ └────────┘ └────────┘
                    │                  │          │          │
                    │                  └──────────┼──────────┘
                    │                             │
                    │                             ▼
                    │                ┌──────────────────────┐
                    └───────────────►│ CLIENT & DATA        │
                                     │ RECONSTRUCTION        │
                                     │                      │
                                     │ Reorder / Reassemble │
                                     │ View / Stream / Save │
                                     └──────────────────────┘
```

---

## Core Design Goals

* **Minimize unnecessary data copies** across network, memory, processing, and storage paths.
* **Reuse buffers** instead of repeatedly allocating and copying large blocks of memory.
* **Use asynchronous I/O** to support high-throughput workloads with reduced blocking.
* **Support Host-to-Host and Host-to-Many data distribution.**
* **Efficiently handle large data streams**, including files, images, and video.
* **Reduce CPU and memory overhead** associated with conventional copy-based data paths.
* **Measure the trade-offs** between traditional copying, low-copy, and zero-copy data paths.
* **Maintain efficient buffer ownership and lifetime management** across different processing stages.
* **Evaluate system performance** using throughput, latency, CPU utilization, memory usage, and I/O metrics.

---

## Technology Stack

### Kernel / Networking

* Linux
* XDP
* eBPF
* AF_XDP
* Sockets

### Memory & Data Path

* C / C++
* Shared Memory
* `mmap`
* Ring Buffers
* Circular Buffers
* Buffer Pools

### Storage & I/O

* `io_uring`
* Linux Filesystem
* Direct I/O
* SSD Storage

### Server & Application Layer

* C / C++
* Java
* TCP / UDP
* HTTP

### Performance Analysis

* `perf`
* Custom benchmarks
* Throughput measurement
* Latency measurement
* CPU utilization
* Memory usage
* Copy-count / data-movement analysis

---

## Data-Sharing Modes

### Host-to-Host

```text
┌──────────┐       ┌──────────────┐       ┌──────────┐
│  Host A  │──────►│    Server    │──────►│  Host B  │
│ Publisher│       │ Data Path    │       │ Receiver │
└──────────┘       └──────────────┘       └──────────┘
```

A participating host sends data through the system, which manages its transmission to another participating host while attempting to minimize unnecessary intermediate copies.

### Host-to-Many

```text
                       ┌──────────┐
                       │ Publisher│
                       │  Host A  │
                       └────┬─────┘
                            │
                            ▼
                     ┌──────────────┐
                     │    Server    │
                     │ Distribution │
                     │    Layer     │
                     └──────┬───────┘
                            │
             ┌──────────────┼──────────────┐
             │              │              │
             ▼              ▼              ▼
        ┌─────────┐    ┌─────────┐    ┌─────────┐
        │ Host B  │    │ Host C  │    │ Host N  │
        └─────────┘    └─────────┘    └─────────┘
```

A host publishes content through the system, allowing multiple participating hosts or users to receive and access the data.

---

## Design Focus

The primary focus of the project is **efficient data movement rather than simply increasing network bandwidth**.

The system investigates how network packets, memory buffers, processing stages, and storage operations can be connected while reducing unnecessary movement of the actual payload.

A key design principle is:

```text
        Move the descriptor
                │
                ▼
      ┌──────────────────┐
      │ Same Data Buffer │
      └──────────────────┘
                │
       Ownership Transfer
                │
                ▼
        Next Processing
             Stage
```

Instead of repeatedly copying the payload between stages, the system attempts to transfer **references/descriptors and ownership of reusable buffers** whenever the workload and architecture permit it.

---

## Performance Evaluation

The system will be evaluated against conventional data-transfer approaches using metrics such as:

| Metric                 | Purpose                                            |
| ---------------------- | -------------------------------------------------- |
| **Throughput**         | Measure total data transferred per second          |
| **End-to-End Latency** | Measure delay from source to destination           |
| **CPU Utilization**    | Measure processing overhead                        |
| **Memory Usage**       | Measure memory requirements and buffer utilization |
| **Data Copies**        | Measure the number and volume of payload copies    |
| **I/O Performance**    | Evaluate asynchronous storage operations           |
| **Buffer Reuse Rate**  | Measure effectiveness of reusable buffers          |
| **Scalability**        | Evaluate performance as clients/data rate increase |

The implementation can be compared across three data paths:

```text
1. Conventional Copy-Based Path
          ↓
2. Low-Copy Path
          ↓
3. Zero-Copy / Shared-Buffer Path
```

This allows the project to quantify the actual benefits and limitations of the proposed architecture rather than assuming that zero-copy is always faster.
