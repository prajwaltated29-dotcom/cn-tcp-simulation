from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import subprocess
import os
import re
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

app = Flask(__name__, static_folder='static')
CORS(app)

NS3_DIR        = os.path.expanduser("~/ns-3-dev")
progress_store = {}
sim_counter    = 0

VARIANTS = ["TcpLinuxReno", "TcpCubic", "TcpBbr"]


# ── Simulation runner ─────────────────────────────────────────────────────────
def run_single_variant(variant, params, mode, script):
    """Run one TCP variant simulation and return (variant, result_dict)."""
    if mode == "wired":
        cmd = (
            f"./ns3 run \"{script}"
            f" --tcpVariant={variant}"
            f" --lossRate={params['lossRate']}"
            f" --bottleneckBw={params['bandwidth']}Mbps"
            f" --bottleneckDelay={params['delay']}ms"
            f" --nSenders={params['nSenders']}"
            f" --nRouters={params.get('nRouters', 2)}"
            f" --simTime={params['simTime']}\""
        )
    else:
        cmd = (
            f"./ns3 run \"{script}"
            f" --tcpVariant={variant}"
            f" --nSenders={params['nSenders']}"
            f" --simTime={params['simTime']}\""
        )

    try:
        proc = subprocess.run(
            cmd, shell=True, cwd=NS3_DIR,
            capture_output=True, text=True, timeout=600
        )
        output = proc.stdout + proc.stderr
        print(f"\n=== {variant} ({mode}) ===")
        print(output[-2000:])

        result = parse_output(output)

        prefix    = "tcp-wired" if mode == "wired" else "tcp-wireless"
        cwnd_file = os.path.join(NS3_DIR, f"{prefix}-{variant}-cwnd.dat")
        result["cwnd"] = load_cwnd(cwnd_file)
        return variant, result

    except subprocess.TimeoutExpired:
        return variant, {"error": "Timed out", "totalThroughput": 0,
                         "avgDelay": 0, "avgLoss": 0, "fairness": 0, "cwnd": []}
    except Exception as e:
        return variant, {"error": str(e), "totalThroughput": 0,
                         "avgDelay": 0, "avgLoss": 0, "fairness": 0, "cwnd": []}


def run_simulation(sim_id, params, mode="wired"):
    script = "scratch/wired-tcp" if mode == "wired" else "scratch/wireless-tcp"

    progress_store[sim_id] = {
        "status": "running",
        "step": 0,
        "total": len(VARIANTS),
        "results": {},
        "completed": [],
        "started_at": time.time()
    }

    # Wireless: run all 3 variants in parallel (3× speedup)
    # Wired: also parallel — each ns3 process is independent
    results = {}

    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = {
            executor.submit(run_single_variant, v, params, mode, script): v
            for v in VARIANTS
        }
        for future in as_completed(futures):
            variant, result = future.result()
            results[variant] = result
            progress_store[sim_id]["results"][variant] = result
            progress_store[sim_id]["completed"].append(variant)
            progress_store[sim_id]["step"] = len(progress_store[sim_id]["completed"])

    progress_store[sim_id]["status"]  = "done"
    progress_store[sim_id]["results"] = results


# ── Output parser ─────────────────────────────────────────────────────────────
def parse_output(output):
    throughputs, delays, losses = [], [], []

    # Match each Flow block individually
    for block in re.finditer(
        r"Flow\s+\d+[^\n]*\n"
        r"(?:.*?\n)*?.*?Throughput\s*:\s*([\d.]+).*?\n"
        r"(?:.*?\n)*?.*?Avg Delay\s*:\s*([\d.]+).*?\n"
        r"(?:.*?\n)*?.*?Loss Ratio\s*:\s*([\d.]+)",
        output
    ):
        th = float(block.group(1))
        dl = float(block.group(2))
        ls = float(block.group(3))
        if th > 0.001:
            throughputs.append(th)
            delays.append(dl)
            losses.append(ls)

    # Fallback: line-by-line parsing
    if not throughputs:
        th_vals, dl_vals, ls_vals = [], [], []
        lines = output.splitlines()
        cur_th = cur_dl = cur_ls = None
        for line in lines:
            m = re.search(r"Throughput\s*:\s*([\d.]+)", line)
            if m: cur_th = float(m.group(1))
            m = re.search(r"Avg Delay\s*:\s*([\d.]+)", line)
            if m: cur_dl = float(m.group(1))
            m = re.search(r"Loss Ratio\s*:\s*([\d.]+)", line)
            if m:
                cur_ls = float(m.group(1))
                if cur_th is not None and cur_dl is not None and cur_th > 0.001:
                    th_vals.append(cur_th)
                    dl_vals.append(cur_dl)
                    ls_vals.append(cur_ls)
                cur_th = cur_dl = cur_ls = None
        throughputs, delays, losses = th_vals, dl_vals, ls_vals

    jfi_m = re.search(r"Jain.s Fairness\s*:\s*([\d.]+)", output)
    jfi   = float(jfi_m.group(1)) if jfi_m else 0.0

    print(f"  Parsed: th={[round(x,3) for x in throughputs]} "
          f"dl={[round(x,1) for x in delays]} "
          f"ls={[round(x,3) for x in losses]} jfi={jfi}")

    if not throughputs:
        print("  WARNING: No flows parsed. Raw output tail:")
        print(output[-2000:])

    return {
        "totalThroughput": round(sum(throughputs), 3),
        "avgDelay":  round(sum(delays) / len(delays),  2) if delays else 0,
        "avgLoss":   round(sum(losses) / len(losses),  3) if losses else 0,
        "fairness":  round(jfi, 3),
        "flows": [{"throughput": t, "delay": d, "loss": l}
                  for t, d, l in zip(throughputs, delays, losses)]
    }


# ── CWND loader ───────────────────────────────────────────────────────────────
def load_cwnd(filepath):
    data = []
    try:
        with open(filepath, "r") as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) == 2:
                    t    = float(parts[0])
                    cwnd = float(parts[1]) / 1024
                    if not data or t - data[-1][0] >= 0.5:
                        data.append([round(t, 1), round(cwnd, 1)])
    except Exception as e:
        print(f"CWND load error {filepath}: {e}")
    return data


# ── Routes ────────────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return send_from_directory("static", "index.html")


@app.route("/simulate", methods=["POST"])
def simulate():
    global sim_counter
    sim_counter += 1
    sim_id = str(sim_counter)

    data   = request.json
    params = data.get("params", data)
    mode   = data.get("mode", "wired")

    progress_store[sim_id] = {"status": "starting"}
    t = threading.Thread(target=run_simulation, args=(sim_id, params, mode))
    t.daemon = True
    t.start()
    return jsonify({"sim_id": sim_id})


@app.route("/progress/<sim_id>")
def progress(sim_id):
    entry = progress_store.get(sim_id, {"status": "not_found"})
    if entry.get("status") == "running":
        elapsed = time.time() - entry.get("started_at", time.time())
        entry = dict(entry, elapsed=round(elapsed))
        if elapsed > 700:
            progress_store[sim_id]["status"] = "error"
            progress_store[sim_id]["error"]  = "Simulation timed out"
            entry = progress_store[sim_id]
    return jsonify(entry)


@app.route("/recommend", methods=["POST"])
def recommend():
    p    = request.json
    bw   = float(p.get("bandwidth", 8))
    dl   = float(p.get("delay", 50))
    loss = float(p.get("lossRate", 0.01)) * 100
    n    = int(p.get("nSenders", 2))
    mode = p.get("mode", "wired")

    if mode == "wireless":
        algo       = "TcpCubic"
        reason     = "Wireless channels have low/zero congestion-related loss. Cubic and Reno both perform well — Cubic edges ahead on higher flow counts."
        confidence = "medium"
    elif loss > 3:
        algo       = "TcpBbr"
        reason     = f"High loss rate ({loss:.1f}%). BBR does not use packet loss as a congestion signal — it will maintain high throughput while Reno and Cubic repeatedly back off."
        confidence = "high"
    elif dl > 150:
        algo       = "TcpBbr"
        reason     = f"Very high delay ({dl}ms) — satellite or intercontinental link. BBR is specifically designed for high bandwidth-delay product networks."
        confidence = "high"
    elif bw > 50 and dl > 30:
        algo       = "TcpCubic"
        reason     = f"High bandwidth ({bw}Mbps) with moderate delay — classic high-BDP network. Cubic's aggressive window growth outperforms Reno significantly here."
        confidence = "high"
    elif n > 5:
        algo       = "TcpBbr"
        reason     = f"Many competing flows ({n}). BBR maintains throughput better under heavy multiplexing than loss-based algorithms."
        confidence = "medium"
    elif loss > 1:
        algo       = "TcpBbr"
        reason     = f"Elevated loss rate ({loss:.1f}%). BBR will outperform Reno and Cubic which both reduce their window on every lost packet."
        confidence = "medium"
    elif bw <= 4:
        algo       = "TcpCubic"
        reason     = f"Low bandwidth ({bw}Mbps) bottleneck. All algorithms converge similarly here but Cubic's faster recovery gives a slight edge."
        confidence = "low"
    else:
        algo       = "TcpCubic"
        reason     = "Moderate network conditions. Cubic is the safe default — most widely deployed and performs reliably across typical networks."
        confidence = "medium"

    return jsonify({"algorithm": algo, "reason": reason, "confidence": confidence})


if __name__ == "__main__":
    app.run(debug=True, port=5000)