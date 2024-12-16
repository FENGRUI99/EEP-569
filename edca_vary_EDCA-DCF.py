import os
import subprocess
import shutil
import signal
import sys
from datetime import datetime
import matplotlib.pyplot as plt
import numpy as np
from enum import Enum

class TrafficTypeEnum(Enum):
    TRAFFIC_DETERMINISTIC = 0
    TRAFFIC_BERNOULLI = 1

def control_c(signum, frame):
    print("exiting")
    sys.exit(1)

signal.signal(signal.SIGINT, control_c)

def main():
    dirname = 'wifi-edca'
    ns3_path = os.path.join('../../../../ns3')
    
    # Check if the ns3 executable exists
    if not os.path.exists(ns3_path):
        print(f"Please run this program from within the correct directory.")
        sys.exit(1)

    results_dir = os.path.join(os.getcwd(), 'results', f"{dirname}-{datetime.now().strftime('%Y%m%d-%H%M%S')}")
    os.makedirs(results_dir, exist_ok=True)

    # Move to ns3 top-level directory
    os.chdir('../../../../')
    
    # Experiment parameters
    configs = [
        {"num_BE": 8, "num_BK": 0, "num_VI": 0, "num_VO": 0, "label": "8_BE_0_BK_0_VI_0_VO"},
        {"num_BE": 2, "num_BK": 2, "num_VI": 2, "num_VO": 2, "label": "2_BE_2_BK_2_VI_2_VO"}
    ]
    rng_run = 1
    max_packets = 1500
    min_lambda = -5
    max_lambda = -2
    step_size = 0.5
    lambdas = [10 ** lam for lam in np.arange(min_lambda, max_lambda + step_size, step_size)]

    data_files = []

    for config in configs:
        num_BE = config["num_BE"]
        num_BK = config["num_BK"]
        num_VI = config["num_VI"]
        num_VO = config["num_VO"]
        label = config["label"]

        edca_dat_file = f"wifi-edca-{label}.dat"
        check_and_remove(edca_dat_file)  # Remove old data file

        # Run the ns3 simulation for each lambda
        for lambda_val in lambdas:
            print(f"Running simulation for {label} with lambda={lambda_val}")
            cmd = (f"./ns3 run 'single-bss-sld-edca --rngRun={rng_run} "
                   f"--payloadSize={max_packets} --perSldLambda={lambda_val} "
                   f"--nSld={num_BE + num_BK + num_VI + num_VO} --nBE={num_BE} "
                   f"--nBK={num_BK} --nVI={num_VI} --nVO={num_VO} "
                   f"--trafficType={TrafficTypeEnum.TRAFFIC_BERNOULLI.value}'")
            subprocess.run(cmd, shell=True)
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
        move_file('wifi-edca.dat', os.path.join(results_dir, edca_dat_file))
        data_files.append(os.path.join(results_dir, edca_dat_file))
    
    # Draw throughput plot
    draw_total_throughput_plot(data_files, results_dir, lambdas, configs)
    draw_e2e_delay_plot(data_files, results_dir, lambdas, configs)
    draw_acc_delay_plot(data_files, results_dir, lambdas, configs)
    draw_queue_delay_plot(data_files, results_dir, lambdas, configs)
    

def draw_total_throughput_plot(data_files, results_dir, lambdas, configs):
    """
    Draw total throughput comparison plot.

    :param data_files: List of paths to EDCA data files
    :param results_dir: Directory to save the plot
    :param lambdas: List of offered load values
    :param configs: Configuration list for labeling
    """
    plt.figure()
    plt.title('Total Throughput Comparison')
    plt.xlabel('Lambda')
    plt.ylabel('Total Throughput (Mbps)')
    plt.grid()
    plt.xscale('log')
    
    for data_file, config in zip(data_files, configs):
        throughput_total = []
        with open(data_file, 'r') as f:
            lines = f.readlines()
            for line in lines:
                tokens = line.split(',')
                throughput_total.append(float(tokens[9]))  # Index 9 is total throughput
        
        plt.plot(lambdas, throughput_total, marker='o', label=config["label"])
    
    plt.legend()
    os.makedirs(results_dir, exist_ok=True)
    plot_file = os.path.join(results_dir, 'wifi-edca-total-throughput-comparison.png')
    plt.savefig(plot_file)
    print(f"Plot saved to {plot_file}")

def draw_e2e_delay_plot(data_files, results_dir, lambdas, configs):
    plt.figure()
    plt.title('End-to-End Delay Comparison')
    plt.xlabel('Lambda')
    plt.ylabel('E2E Delay (ms)')
    plt.grid()
    plt.xscale('log')
    
    for data_file, config in zip(data_files, configs):
        e2e_delay = []
        with open(data_file, 'r') as f:
            lines = f.readlines()
            for line in lines:
                tokens = line.split(',')
                e2e_delay.append(float(tokens[24]))  # Adjust index as per the data format
        
        plt.plot(lambdas, e2e_delay, marker='o', label=config["label"])
    
    plt.legend()
    os.makedirs(results_dir, exist_ok=True)
    plot_file = os.path.join(results_dir, 'wifi-edca-e2e-delay-comparison.png')
    plt.savefig(plot_file)
    print(f"Plot saved to {plot_file}")

def draw_acc_delay_plot(data_files, results_dir, lambdas, configs):
    plt.figure()
    plt.title('Access Delay Comparison')
    plt.xlabel('Lambda')
    plt.ylabel('Access Delay (ms)')
    plt.grid()
    plt.xscale('log')
    
    for data_file, config in zip(data_files, configs):
        acc_delay = []
        with open(data_file, 'r') as f:
            lines = f.readlines()
            for line in lines:
                tokens = line.split(',')
                acc_delay.append(float(tokens[19]))  # Adjust index as per the data format
        
        plt.plot(lambdas, acc_delay, marker='o', label=config["label"])
    
    plt.legend()
    os.makedirs(results_dir, exist_ok=True)
    plot_file = os.path.join(results_dir, 'wifi-edca-acc-delay-comparison.png')
    plt.savefig(plot_file)
    print(f"Plot saved to {plot_file}")

def draw_queue_delay_plot(data_files, results_dir, lambdas, configs):
    plt.figure()
    plt.title('Queue Delay Comparison')
    plt.xlabel('Lambda')
    plt.ylabel('Queue Delay (ms)')
    plt.grid()
    plt.xscale('log')
    
    for data_file, config in zip(data_files, configs):
        queue_delay = []
        with open(data_file, 'r') as f:
            lines = f.readlines()
            for line in lines:
                tokens = line.split(',')
                queue_delay.append(float(tokens[14]))  # Adjust index as per the data format
        
        plt.plot(lambdas, queue_delay, marker='o', label=config["label"])
    
    plt.legend()
    os.makedirs(results_dir, exist_ok=True)
    plot_file = os.path.join(results_dir, 'wifi-edca-queue-delay-comparison.png')
    plt.savefig(plot_file)
    print(f"Plot saved to {plot_file}")

def check_and_remove(filename):
    if os.path.exists(filename):
        os.remove(filename)
        print(f"Removed old file: {filename}")

def move_file(filename, destination):
    if os.path.exists(filename):
        shutil.move(filename, destination)

if __name__ == "__main__":
    main()