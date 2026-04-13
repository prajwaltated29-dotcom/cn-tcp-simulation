import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

variants = ['TCP Reno', 'TCP Cubic', 'TCP BBR']
colors   = ['#4C72B0', '#55A868', '#C44E52']

# ── Wired results (data flows only) ──────────────────────────────────────────
w_throughput = [0.499 + 0.436, 1.152 + 1.236, 3.724 + 3.811]
w_delay      = [(62.71 + 62.80)/2, (62.78 + 62.72)/2, (131.25 + 137.27)/2]
w_loss       = [(0.997 + 0.790)/2, (0.937 + 0.855)/2, (0.869 + 0.998)/2]
w_fairness   = [0.517, 0.520, 0.530]

# ── Wireless results (data flows only) ───────────────────────────────────────
wl_throughput = [7.876 + 7.837, 7.876 + 7.837, 7.055 + 7.605]
wl_delay      = [(86.87 + 86.90)/2, (86.87 + 86.90)/2, (84.25 + 84.97)/2]
wl_loss       = [0.0, 0.0, 0.0]
wl_fairness   = [0.523, 0.523, 0.521]

# ════════════════════════════════════════════════════════════════════════════
# Figure 1 — Wired comparison
# ════════════════════════════════════════════════════════════════════════════
fig, axes = plt.subplots(2, 2, figsize=(12, 8))
fig.suptitle('TCP Congestion Control – Wired Network (NS-3.42)',
             fontsize=14, fontweight='bold')

def bar_chart(ax, data, title, ylabel, ylim_factor=1.2,
              hline=None, hline_label=None, fmt='{:.2f}'):
    bars = ax.bar(variants, data, color=colors, edgecolor='black', linewidth=0.7)
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    ax.set_ylim(0, max(data) * ylim_factor)
    for bar, val in zip(bars, data):
        ax.text(bar.get_x() + bar.get_width()/2,
                bar.get_height() + max(data)*0.02,
                fmt.format(val), ha='center', va='bottom', fontsize=10)
    if hline is not None:
        ax.axhline(y=hline, color='red', linestyle='--',
                   linewidth=1, label=hline_label)
        ax.legend(fontsize=8)

bar_chart(axes[0,0], w_throughput, 'Total Throughput', 'Throughput (Mbps)',
          hline=8, hline_label='Link capacity (8 Mbps)')
bar_chart(axes[0,1], w_delay,      'Avg End-to-End Delay', 'Delay (ms)', fmt='{:.1f}')
bar_chart(axes[1,0], w_loss,       'Avg Packet Loss Ratio', 'Loss (%)',
          ylim_factor=2.0, fmt='{:.3f}%')

bars = axes[1,1].bar(variants, w_fairness, color=colors,
                     edgecolor='black', linewidth=0.7)
axes[1,1].set_title("Jain's Fairness Index")
axes[1,1].set_ylabel('Fairness (0–1)')
axes[1,1].set_ylim(0, 1.15)
axes[1,1].axhline(y=1.0, color='green', linestyle='--',
                  linewidth=1, label='Perfect fairness')
axes[1,1].legend(fontsize=8)
for bar, val in zip(bars, w_fairness):
    axes[1,1].text(bar.get_x() + bar.get_width()/2,
                   bar.get_height() + 0.01,
                   f'{val:.3f}', ha='center', va='bottom', fontsize=10)

plt.tight_layout()
plt.savefig('tcp_wired_comparison.png', dpi=150, bbox_inches='tight')
print("Saved: tcp_wired_comparison.png")

# ════════════════════════════════════════════════════════════════════════════
# Figure 2 — Wireless comparison
# ════════════════════════════════════════════════════════════════════════════
fig2, axes2 = plt.subplots(2, 2, figsize=(12, 8))
fig2.suptitle('TCP Congestion Control – Wireless Network (NS-3.42)',
              fontsize=14, fontweight='bold')

bar_chart(axes2[0,0], wl_throughput, 'Total Throughput', 'Throughput (Mbps)',
          ylim_factor=1.3)
bar_chart(axes2[0,1], wl_delay,      'Avg End-to-End Delay', 'Delay (ms)', fmt='{:.1f}')
bar_chart(axes2[1,0], [0.001, 0.001, 0.001],
          'Avg Packet Loss Ratio', 'Loss (%)',
          ylim_factor=10, fmt='{:.3f}%')
axes2[1,0].set_ylim(0, 0.05)
for i, ax_bar in enumerate(axes2[1,0].patches):
    axes2[1,0].text(ax_bar.get_x() + ax_bar.get_width()/2,
                    0.002, '0.000%', ha='center', va='bottom', fontsize=10)

bars2 = axes2[1,1].bar(variants, wl_fairness, color=colors,
                        edgecolor='black', linewidth=0.7)
axes2[1,1].set_title("Jain's Fairness Index")
axes2[1,1].set_ylabel('Fairness (0–1)')
axes2[1,1].set_ylim(0, 1.15)
axes2[1,1].axhline(y=1.0, color='green', linestyle='--',
                   linewidth=1, label='Perfect fairness')
axes2[1,1].legend(fontsize=8)
for bar, val in zip(bars2, wl_fairness):
    axes2[1,1].text(bar.get_x() + bar.get_width()/2,
                    bar.get_height() + 0.01,
                    f'{val:.3f}', ha='center', va='bottom', fontsize=10)

plt.tight_layout()
plt.savefig('tcp_wireless_comparison.png', dpi=150, bbox_inches='tight')
print("Saved: tcp_wireless_comparison.png")

# ════════════════════════════════════════════════════════════════════════════
# Figure 3 — Wired vs Wireless throughput comparison
# ════════════════════════════════════════════════════════════════════════════
fig3, ax3 = plt.subplots(figsize=(10, 5))
x = np.arange(len(variants))
width = 0.35
bars_w  = ax3.bar(x - width/2, w_throughput,  width, label='Wired',
                  color=colors, edgecolor='black', linewidth=0.7, alpha=0.9)
bars_wl = ax3.bar(x + width/2, wl_throughput, width, label='Wireless',
                  color=colors, edgecolor='black', linewidth=0.7, alpha=0.5,
                  hatch='//')
ax3.set_title('Wired vs Wireless – Total Throughput Comparison', fontsize=13)
ax3.set_ylabel('Throughput (Mbps)')
ax3.set_xticks(x)
ax3.set_xticklabels(variants)
ax3.legend()
ax3.set_ylim(0, max(wl_throughput) * 1.2)
for bar in bars_w:
    ax3.text(bar.get_x() + bar.get_width()/2,
             bar.get_height() + 0.2,
             f'{bar.get_height():.2f}', ha='center', va='bottom', fontsize=9)
for bar in bars_wl:
    ax3.text(bar.get_x() + bar.get_width()/2,
             bar.get_height() + 0.2,
             f'{bar.get_height():.2f}', ha='center', va='bottom', fontsize=9)
plt.tight_layout()
plt.savefig('tcp_wired_vs_wireless.png', dpi=150, bbox_inches='tight')
print("Saved: tcp_wired_vs_wireless.png")

# ════════════════════════════════════════════════════════════════════════════
# Figure 4 — CWND over time (wired)
# ════════════════════════════════════════════════════════════════════════════
fig4, ax4 = plt.subplots(figsize=(10, 4))
cwnd_files = {
    'TCP Reno':  'tcp-wired-TcpLinuxReno-cwnd.dat',
    'TCP Cubic': 'tcp-wired-TcpCubic-cwnd.dat',
    'TCP BBR':   'tcp-wired-TcpBbr-cwnd.dat',
}
for (label, fname), color in zip(cwnd_files.items(), colors):
    try:
        data = np.loadtxt(fname)
        if data.ndim == 1:
            data = data.reshape(1, -1)
        ax4.plot(data[:, 0], data[:, 1]/1024,
                 label=label, color=color, linewidth=1)
    except Exception as e:
        print(f"Could not load {fname}: {e}")
ax4.set_title('Congestion Window (CWND) Over Time – Wired')
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('CWND (KB)')
ax4.legend()
ax4.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig('tcp_wired_cwnd.png', dpi=150, bbox_inches='tight')
print("Saved: tcp_wired_cwnd.png")

# ════════════════════════════════════════════════════════════════════════════
# Figure 5 — CWND over time (wireless)
# ════════════════════════════════════════════════════════════════════════════
fig5, ax5 = plt.subplots(figsize=(10, 4))
cwnd_files_wl = {
    'TCP Reno':  'tcp-wireless-TcpLinuxReno-cwnd.dat',
    'TCP Cubic': 'tcp-wireless-TcpCubic-cwnd.dat',
    'TCP BBR':   'tcp-wireless-TcpBbr-cwnd.dat',
}
for (label, fname), color in zip(cwnd_files_wl.items(), colors):
    try:
        data = np.loadtxt(fname)
        if data.ndim == 1:
            data = data.reshape(1, -1)
        ax5.plot(data[:, 0], data[:, 1]/1024,
                 label=label, color=color, linewidth=1)
    except Exception as e:
        print(f"Could not load {fname}: {e}")
ax5.set_title('Congestion Window (CWND) Over Time – Wireless')
ax5.set_xlabel('Time (s)')
ax5.set_ylabel('CWND (KB)')
ax5.legend()
ax5.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig('tcp_wireless_cwnd.png', dpi=150, bbox_inches='tight')
print("Saved: tcp_wireless_cwnd.png")