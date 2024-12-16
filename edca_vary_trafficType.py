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

def draw_comparison_plots(edca_dat_file_0, edca_dat_file_1, results_dir, lambdas):
    """
    Compare throughput and delay between trafficType=0 and trafficType=1.

    :param edca_dat_file_0: Data file for trafficType=0
    :param edca_dat_file_1: Data file for trafficType=1
    :param results_dir: Directory to save the plot
    :param lambdas: List of offered load values
    """
    throughput_0, delay_0 = extract_metrics(edca_dat_file_0)
    throughput_1, delay_1 = extract_metrics(edca_dat_file_1)

    # Plot throughput comparison
    plt.figure()
    plt.title('Throughput vs. Offered Load (TrafficType=0 and 1)')
    plt.xlabel('Lambda')
    plt.ylabel('Throughput (Mbps)')
    plt.grid()
    plt.xscale('log')
    plt.plot(lambdas, throughput_0, marker='o', label='TrafficType=0')
    plt.plot(lambdas, throughput_1, marker='x', label='TrafficType=1')
    plt.legend()
    os.makedirs(results_dir, exist_ok=True)
    plt.savefig(os.path.join(results_dir, 'wifi-edca-thrp-comparison.png'))
    print(f"Throughput comparison plot saved to {os.path.join(results_dir, 'wifi-edca-thrp-comparison.png')}")

    # Plot end-to-end delay comparison
    plt.figure()
    plt.title('End-to-End Delay vs. Offered Load (TrafficType=0 and 1)')
    plt.xlabel('Lambda')
    plt.ylabel('End-to-End Delay (ms)')
    plt.grid()
    plt.xscale('log')
    plt.plot(lambdas, delay_0, marker='o', label='TrafficType=0')
    plt.plot(lambdas, delay_1, marker='x', label='TrafficType=1')
    plt.legend()
    plt.savefig(os.path.join(results_dir, 'wifi-edca-delay-comparison.png'))
    print(f"End-to-End delay comparison plot saved to {os.path.join(results_dir, 'wifi-edca-delay-comparison.png')}")

def extract_metrics(edca_dat_file):
    """
    Extract throughput and delay metrics from the data file.

    :param edca_dat_file: Path to the EDCA data file
    :return: (throughput, end-to-end delay) as lists
    """
    throughput = []
    delay = []

    with open(edca_dat_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            tokens = line.split(',')
            throughput.append(float(tokens[9]))  # Total throughput
            delay.append(float(tokens[19]))  # Average end-to-end delay

    return throughput, delay

def check_and_remove(filename):
    if os.path.exists(filename):
        response = input(f"Remove existing file {filename}? [Yes/No]: ").strip().lower()
        if response == 'yes':
            os.remove(filename)
            print(f"Removed {filename}")
        else:
            print("Exiting...")
            sys.exit(1)

def main():
    dirname = 'wifi-edca'
    ns3_path = os.path.join('../../../../ns3')
    
    # Check if the ns3 executable exists
    if not os.path.exists(ns3_path):
        print(f"Please run this program from within the correct directory.")
        sys.exit(1)

    results_dir = os.path.join(os.getcwd(), 'results', f"{dirname}-{datetime.now().strftime('%Y%m%d-%H%M%S')}")
    os.system('mkdir -p ' + results_dir)


    # Move to ns3 top-level directory
    os.chdir('../../../../')

    

    # Check for existing data files and prompt for removal
    check_and_remove('wifi-edca.dat')
    

    # Check for existing data files and prompt for removal
    check_and_remove('wifi-edca.dat')

    # 实验参数
    rng_run = 1
    max_packets = 1500
    num_STA = 8
    num_BE = 2
    num_BK = 2
    num_VI = 2
    num_VO = 2
    min_lambda = -5
    max_lambda = -2
    step_size = 0.5
    lambdas = np.arange(min_lambda, max_lambda + step_size, step_size)
    lambda_values = [10 ** lam for lam in lambdas]

    # 运行两种 trafficType 的仿真
    traffic_types = [0, 1]
    results_files = {}

    for trafficType in traffic_types:
        output_file = os.path.join(results_dir, f'wifi-edca-{trafficType}.dat')
        for lambda_val in lambda_values:
            print(f"Running simulation for trafficType={trafficType}, lambda={lambda_val}")
            cmd = f"./ns3 run 'single-bss-sld-edca --rngRun={rng_run} --payloadSize={max_packets} --perSldLambda={lambda_val} --nSld={num_STA} --nBE={num_BE} --nBK={num_BK} --nVI={num_VI} --nVO={num_VO} --trafficType={trafficType}'"
            subprocess.run(cmd, shell=True)

        # 移动生成的结果文件到对应目录
        if not os.path.exists('wifi-edca.dat'):
            print(f"Error: Simulation for trafficType={trafficType} did not produce wifi-edca.dat.")
            sys.exit(1)
        shutil.move('wifi-edca.dat', output_file)
        results_files[trafficType] = output_file

    # 画图对比 trafficType=0 和 trafficType=1 的吞吐量和延迟
    draw_comparison_plots(results_files[0], results_files[1], results_dir, lambda_values)

    # 保存当前 Git 提交信息
    with open(os.path.join(results_dir, 'git-commit.txt'), 'w') as f:
        commit_info = subprocess.run(['git', 'show', '--name-only'], stdout=subprocess.PIPE)
        f.write(commit_info.stdout.decode())
    print(f"Results saved in {results_dir}")

if __name__ == "__main__":
    main()