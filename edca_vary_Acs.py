import os
import subprocess
import shutil
import signal
import sys
from datetime import datetime
import matplotlib.pyplot as plt
import numpy as np

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
    os.system('mkdir -p ' + results_dir)


    # Move to ns3 top-level directory
    os.chdir('../../../../')
    

    # Check for existing data files and prompt for removal
    check_and_remove('wifi-edca.dat')

    # Experiment parameters
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
    lambdas = []
    # Run the ns3 simulation for each distance
    for lam in np.arange(min_lambda, max_lambda + step_size, step_size):
        lambda_val = 10 ** lam  
        lambdas.append(lambda_val)
        print(lambda_val)
        cmd = f"./ns3 run 'single-bss-sld-edca --rngRun={rng_run} --payloadSize={max_packets} --perSldLambda={lambda_val} --nSld={num_STA} --nBE={num_BE} --nBK={num_BK} --nVI={num_VI} --nVO={num_VO}'"
        subprocess.run(cmd, shell=True)
    move_file('wifi-edca.dat', results_dir)

    # Plot the results
    edca_dat_file = os.path.join(results_dir, 'wifi-edca.dat')
    draw_throughput_plot(edca_dat_file, results_dir, lambdas)
    draw_e2e_delay_plot(edca_dat_file, results_dir, lambdas)
    draw_acc_delay_plot(edca_dat_file, results_dir, lambdas)
    draw_queue_delay_plot(edca_dat_file, results_dir, lambdas)
    # Save the git commit information
    with open(os.path.join(results_dir, 'git-commit.txt'), 'w') as f:
        commit_info = subprocess.run(['git', 'show', '--name-only'], stdout=subprocess.PIPE)
        f.write(commit_info.stdout.decode())

def draw_throughput_plot(edca_dat_file, results_dir, lambdas):
    """
    Draw throughput vs. offered load plot from the given data file.

    :param edca_dat_file: Path to the EDCA data file
    :param results_dir: Directory to save the plot
    :param lambdas: List of offered load values
    """
    plt.figure()
    plt.title('Throughput vs. Offered Load')
    plt.xlabel('Lambda')
    plt.ylabel('Throughput (Mbps)')
    plt.grid()
    plt.xscale('log')
    
    throughput_BE = []
    throughput_BK = []
    throughput_VI = []
    throughput_VO = []
    throughput_total = []
    
    with open(edca_dat_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            tokens = line.split(',')
            throughput_BE.append(float(tokens[5]))
            throughput_BK.append(float(tokens[6]))
            throughput_VI.append(float(tokens[7]))
            throughput_VO.append(float(tokens[8]))
            throughput_total.append(float(tokens[9]))
    
    plt.plot(lambdas, throughput_BE, marker='o', label='BE')
    plt.plot(lambdas, throughput_BK, marker='x', label='BK')
    plt.plot(lambdas, throughput_VI, marker='o', label='VI')
    plt.plot(lambdas, throughput_VO, marker='x', label='VO')
    plt.plot(lambdas, throughput_total, marker='^', label='Total')
    plt.legend()

    os.makedirs(results_dir, exist_ok=True)
    plt.savefig(os.path.join(results_dir, 'wifi-edca-thrp.png'))
    print(f"Plot saved to {os.path.join(results_dir, 'wifi-edca-thrp.png')}")

def draw_e2e_delay_plot(edca_dat_file, results_dir, lambdas):
    """
    Draw end-to-end delay vs. offered load plot from the given data file.

    :param edca_dat_file: Path to the EDCA data file
    :param results_dir: Directory to save the plot
    :param lambdas: List of offered load values
    """
    plt.figure()
    plt.title('End-to-End Delay vs. Offered Load')
    plt.xlabel('Lambda')
    plt.ylabel('End-to-End Delay (ms)')
    plt.grid()
    plt.xscale('log')
    
    delay_BE = []
    delay_BK = []
    delay_VI = []
    delay_VO = []
    
    with open(edca_dat_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            tokens = line.split(',')
            delay_BE.append(float(tokens[20]))
            delay_BK.append(float(tokens[21]))
            delay_VI.append(float(tokens[22]))
            delay_VO.append(float(tokens[23]))
    
    plt.plot(lambdas, delay_BE, marker='o', label='BE')
    plt.plot(lambdas, delay_BK, marker='x', label='BK')
    plt.plot(lambdas, delay_VI, marker='o', label='VI')
    plt.plot(lambdas, delay_VO, marker='x', label='VO')
    plt.legend()

    os.makedirs(results_dir, exist_ok=True)
    plt.savefig(os.path.join(results_dir, 'wifi-edca-delay.png'))
    print(f"Plot saved to {os.path.join(results_dir, 'wifi-edca-delay.png')}")

def draw_acc_delay_plot(edca_dat_file, results_dir, lambdas):
    """
    Draw access delay vs. offered load plot from the given data file.

    :param edca_dat_file: Path to the EDCA data file
    :param results_dir: Directory to save the plot
    :param lambdas: List of offered load values
    """
    plt.figure()
    plt.title('Access Delay vs. Offered Load')
    plt.xlabel('Lambda')
    plt.ylabel('Access Delay (ms)')
    plt.grid()
    plt.xscale('log')
    
    acc_delay_BE = []
    acc_delay_BK = []
    acc_delay_VI = []
    acc_delay_VO = []
    
    with open(edca_dat_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            tokens = line.split(',')
            acc_delay_BE.append(float(tokens[15]))
            acc_delay_BK.append(float(tokens[16]))
            acc_delay_VI.append(float(tokens[17]))
            acc_delay_VO.append(float(tokens[18]))
    
    plt.plot(lambdas, acc_delay_BE, marker='o', label='BE')
    plt.plot(lambdas, acc_delay_BK, marker='x', label='BK')
    plt.plot(lambdas, acc_delay_VI, marker='o', label='VI')
    plt.plot(lambdas, acc_delay_VO, marker='x', label='VO')
    plt.legend()
    
    os.makedirs(results_dir, exist_ok=True)
    plt.savefig(os.path.join(results_dir, 'wifi-edca-acc-delay.png'))
    print(f"Plot saved to {os.path.join(results_dir, 'wifi-edca-acc-delay.png')}")

def draw_queue_delay_plot(edca_dat_file, results_dir, lambdas):
    """
    Draw queue delay vs. offered load plot from the given data file.

    :param edca_dat_file: Path to the EDCA data file
    :param results_dir: Directory to save the plot
    :param lambdas: List of offered load values
    """
    plt.figure()
    plt.title('Queue Delay vs. Offered Load')
    plt.xlabel('Lambda')
    plt.ylabel('Queue Delay (ms)')
    plt.grid()
    plt.xscale('log')
    
    queue_delay_BE = []
    queue_delay_BK = []
    queue_delay_VI = []
    queue_delay_VO = []
    
    with open(edca_dat_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            tokens = line.split(',')
            queue_delay_BE.append(float(tokens[10]))
            queue_delay_BK.append(float(tokens[11]))
            queue_delay_VI.append(float(tokens[12]))
            queue_delay_VO.append(float(tokens[13]))
    
    plt.plot(lambdas, queue_delay_BE, marker='o', label='BE')
    plt.plot(lambdas, queue_delay_BK, marker='x', label='BK')
    plt.plot(lambdas, queue_delay_VI, marker='o', label='VI')
    plt.plot(lambdas, queue_delay_VO, marker='x', label='VO')
    plt.legend()

    os.makedirs(results_dir, exist_ok=True)
    plt.savefig(os.path.join(results_dir, 'wifi-edca-queue-delay.png'))
    print(f"Plot saved to {os.path.join(results_dir, 'wifi-edca-queue-delay.png')}")

def check_and_remove(filename):
    if os.path.exists(filename):
        response = input(f"Remove existing file {filename}? [Yes/No]: ").strip().lower()
        if response == 'yes':
            os.remove(filename)
            print(f"Removed {filename}")
        else:
            print("Exiting...")
            sys.exit(1)

def move_file(filename, destination_dir):
    if os.path.exists(filename):
        shutil.move(filename, destination_dir)

if __name__ == "__main__":
    main()
