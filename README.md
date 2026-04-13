# cn-tcp-simulation
NS-3 based simulation and comparison of TCP congestion control algorithms (Reno, CUBIC, BBR) with a Flask web app for live simulation and visualization.
# CN TCP Simulation

A network simulation project comparing **TCP congestion control algorithms** (Reno, CUBIC, BBR) using **NS-3**, with a **Flask web app** for live simulation, interactive visualization, and PDF report export.

# CN TCP Simulation

A network simulation project comparing TCP congestion control algorithms — Reno, CUBIC, and BBR — using NS-3, with a Flask web application for live simulation, interactive visualization, and PDF report export.

---

## Project Overview

Developed as part of the Communication Networks Lab at Manipal Institute of Technology, this project simulates and analyzes the behavior of three major TCP congestion control algorithms under various network conditions using the NS-3 network simulator.

---

## Features

- NS-3 simulations for TCP Reno, CUBIC, and BBR
- Interactive charts (Throughput, Delay, Packet Loss, CWND, Jain's Fairness Index) using Chart.js
- Flask web application to configure and run simulations from the browser
- Algorithm advisor that recommends the best TCP variant based on network conditions
- PDF export of simulation results

---

## Project Structure

```
cn-tcp-simulation/
│
├── app.py               # Flask backend — runs simulations and serves results
├── index.html           # Frontend UI with Chart.js visualizations
├── plot-script.py       # Script for generating result plots
├── wired-tcp.cc         # NS-3 simulation script for wired network
├── wireless-tcp.cc      # NS-3 simulation script for wireless network
├── .gitignore
├── LICENSE
└── README.md
```

---

## Requirements

- Python 3.8+
- Flask
- NS-3 (installed on WSL2/Ubuntu)
- matplotlib, pandas, numpy

Install Python dependencies:
```bash
pip install flask matplotlib pandas numpy
```

---

## How to Run

**1. Clone the repository**
```bash
git clone https://github.com/prajwaltated29-dotcom/cn-tcp-simulation.git
cd cn-tcp-simulation
```

**2. Run the Flask app**
```bash
python app.py
```

**3. Open in browser**
```
http://localhost:5000
```

> NS-3 must be installed and configured on your system (tested on WSL2 Ubuntu) for live simulation to work.

---

## Metrics Compared

| Metric | Description |
|---|---|
| Throughput | Data successfully delivered per second |
| End-to-End Delay | Time taken for packets to reach destination |
| Packet Loss | Percentage of packets dropped |
| CWND | Congestion window size over time |
| Jain's Fairness Index | How fairly bandwidth is shared between flows |

---

## Algorithm Summary

| Algorithm | Best For |
|---|---|
| TCP Reno | Low-loss, stable networks |
| TCP CUBIC | High-bandwidth, high-latency networks (default on most Linux systems) |
| TCP BBR | Networks with bufferbloat or variable delay |

---

## Author

**Prajwal Tat**  
Electronics and Communication Engineering, MIT Manipal  
Founding Hardware Engineer, Aerionix / Redinium Technologies

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
