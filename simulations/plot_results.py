"""
Plot ODA-MD vs OD comparison graphs (paper-style)
Creates cumulative detection curves showing outliers detected over time
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Get script directory (where CSV files are located)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Style settings for publication-quality plots
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams['font.size'] = 12
plt.rcParams['axes.labelsize'] = 14
plt.rcParams['axes.titlesize'] = 14
plt.rcParams['legend.fontsize'] = 11
plt.rcParams['figure.figsize'] = (10, 6)

def load_metrics(filename):
    """Load metrics CSV file"""
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found")
        return None
    return pd.read_csv(filename)

def plot_detection_accuracy(odamd_file, od_file, output_file='fig4_detection_accuracy.png'):
    """
    Fig 4: Detection Accuracy vs Simulation Time
    Paper formula: DA = CumulativeTP / TotalOutliers(1000) × 100%
    """
    df_odamd = load_metrics(odamd_file)
    df_od = load_metrics(od_file)
    
    TOTAL_OUTLIERS = 1000  # Paper: 1000 outliers injected
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    if df_odamd is not None:
        da_percent = (df_odamd['CumulativeTP'] / TOTAL_OUTLIERS) * 100
        ax.plot(df_odamd['Time'], da_percent, 
                'b-o', linewidth=2, markersize=6, 
                label='ODA-MD (Proposed)', markevery=max(1, len(df_odamd)//10))
    
    if df_od is not None:
        da_percent = (df_od['CumulativeTP'] / TOTAL_OUTLIERS) * 100
        ax.plot(df_od['Time'], da_percent, 
                'r-s', linewidth=2, markersize=6, 
                label='OD [Fawzy et al.]', markevery=max(1, len(df_od)//10))
    
    ax.set_xlabel('Simulation Time (s)')
    ax.set_ylabel('Detection Accuracy (%)')
    ax.set_title('Detection Accuracy vs. Simulation Time')
    ax.legend(loc='lower right')
    ax.set_xlim(left=0)
    ax.set_ylim(0, 105)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()

def plot_false_alarm_rate(odamd_file, od_file, output_file='fig5_false_alarm_rate.png'):
    """
    Fig 5: False Alarm Rate vs Simulation Time
    FAR = FP / (FP + TN) × 100 - shows how FAR evolves over time
    """
    df_odamd = load_metrics(odamd_file)
    df_od = load_metrics(od_file)
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    if df_odamd is not None:
        ax.plot(df_odamd['Time'], df_odamd['FAR'] * 100, 
                'b-o', linewidth=2, markersize=6, 
                label='ODA-MD (Proposed)', markevery=max(1, len(df_odamd)//10))
    
    if df_od is not None:
        ax.plot(df_od['Time'], df_od['FAR'] * 100, 
                'r-s', linewidth=2, markersize=6, 
                label='OD [Fawzy et al.]', markevery=max(1, len(df_od)//10))
    
    ax.set_xlabel('Simulation Time (s)')
    ax.set_ylabel('False Alarm Rate (%)')
    ax.set_title('False Alarm Rate vs. Simulation Time')
    ax.legend(loc='upper right')
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    
    # Add grid for better readability
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()

def plot_detection_accuracy_percentage(odamd_file, od_file, output_file='fig4b_da_percentage.png'):
    """
    Alternative Fig 4: Detection Accuracy (%) vs Simulation Time
    """
    df_odamd = load_metrics(odamd_file)
    df_od = load_metrics(od_file)
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    if df_odamd is not None:
        ax.plot(df_odamd['Time'], df_odamd['DA'] * 100, 
                'b-o', linewidth=2, markersize=6, 
                label='ODA-MD (Proposed)', markevery=max(1, len(df_odamd)//10))
    
    if df_od is not None:
        ax.plot(df_od['Time'], df_od['DA'] * 100, 
                'r-s', linewidth=2, markersize=6, 
                label='OD [Fawzy et al.]', markevery=max(1, len(df_od)//10))
    
    ax.set_xlabel('Simulation Time (s)')
    ax.set_ylabel('Detection Accuracy (%)')
    ax.set_title('Detection Accuracy vs. Simulation Time')
    ax.legend(loc='lower right')
    ax.set_xlim(left=0)
    ax.set_ylim(0, 105)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()

def create_comparison_table(odamd_file, od_file):
    """Print final comparison table"""
    df_odamd = load_metrics(odamd_file)
    df_od = load_metrics(od_file)
    
    print("\n" + "="*60)
    print("              FINAL COMPARISON TABLE")
    print("="*60)
    print(f"{'Metric':<25} {'ODA-MD':>15} {'OD':>15}")
    print("-"*60)
    
    if df_odamd is not None:
        odamd_da = df_odamd['DA'].iloc[-1] * 100
        odamd_far = df_odamd['FAR'].iloc[-1] * 100
        odamd_tp = df_odamd['CumulativeTP'].iloc[-1]
    else:
        odamd_da, odamd_far, odamd_tp = 0, 0, 0
        
    if df_od is not None:
        od_da = df_od['DA'].iloc[-1] * 100
        od_far = df_od['FAR'].iloc[-1] * 100
        od_tp = df_od['CumulativeTP'].iloc[-1]
    else:
        od_da, od_far, od_tp = 0, 0, 0
    
    print(f"{'Detection Accuracy (%)':<25} {odamd_da:>14.2f}% {od_da:>14.2f}%")
    print(f"{'False Alarm Rate (%)':<25} {odamd_far:>14.2f}% {od_far:>14.2f}%")
    print(f"{'Total Outliers Detected':<25} {odamd_tp:>15} {od_tp:>15}")
    print("="*60)

def main():
    # File paths (relative to script directory)
    odamd_file = os.path.join(SCRIPT_DIR, 'metrics_odamd.csv')
    od_file = os.path.join(SCRIPT_DIR, 'metrics_od.csv')
    
    print("Generating paper-style comparison graphs...")
    print(f"Looking for CSV files in: {SCRIPT_DIR}")
    
    # Generate all plots (save to script directory)
    plot_detection_accuracy(odamd_file, od_file, 
                           os.path.join(SCRIPT_DIR, 'fig4_detection_accuracy.png'))
    plot_false_alarm_rate(odamd_file, od_file, 
                         os.path.join(SCRIPT_DIR, 'fig5_false_alarm_rate.png'))
    plot_detection_accuracy_percentage(odamd_file, od_file, 
                                       os.path.join(SCRIPT_DIR, 'fig4b_da_percentage.png'))
    
    # Print comparison table
    create_comparison_table(odamd_file, od_file)
    
    print(f"\nDone! Check the generated PNG files in: {SCRIPT_DIR}")

if __name__ == "__main__":
    main()
