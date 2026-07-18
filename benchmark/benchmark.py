import argparse
import json
import re
import subprocess
from pathlib import Path

binaryDirectories = {
    'native': 'bin/x86',
    'sbt': 'translated',
    'emu': 'bin/riscv',
    'qemu': 'bin/riscv'
}

runnerPaths = {
    'qemu': 'qemu-riscv32-static',
    'emu': '../cmake-build-debug-wsl/emulator/emulator'
}

perfArgs = [ 'task-clock', 'instructions', 'branch-misses', 'branches', 'cpu-cycles', 'cache-references', 'cache-misses' ]

parser = argparse.ArgumentParser(description='Benchmark runner')
parser.add_argument('--tool', action="store", dest='tool', default='native', help='Tool which to benchmark against.', choices=['native', 'sbt', 'emu', 'qemu'])
parser.add_argument('--name', action="store", dest='name', default='run', help='Name of the benchmarking run, used for filename.')
args = parser.parse_args()

def extractBenchmarkRuntime(outputText):
    outputText = outputText.strip()

    if not outputText:
        return None

    if re.fullmatch(r'\d+(?:\.\d+)?', outputText):
        return float(outputText)

    totalTimeMatch = re.search(r'Total time \(secs\):\s*(\d+(?:\.\d+)?)', outputText)
    if totalTimeMatch is not None:
        return float(totalTimeMatch.group(1))

    return None


def loadPerfOutputJson(resultPath):
    with open(resultPath, 'r', encoding='utf-8') as resultFile:
        resultLines = [line.strip() for line in resultFile if line.strip()]

    fixedJson = '[\n' + ',\n'.join(resultLines) + '\n]'
    original = json.loads(fixedJson)
    data = {}

    for perf in original:
        if perf.get('event'):
            data[perf['event']] = perf
            data[perf['event']]['counter-value'] = float(perf['counter-value'])
            data[perf['event']]['metric-value'] = float(perf['metric-value'])

    return data

def processRun(name, perf, runtime, command):
    entry = {
        'name': name,
        'bench_runtime': runtime * 1000,
        'command': ' '.join(command)
    }

    entry['total_runtime'] = perf['task-clock']['counter-value']
    entry['instructions_ran'] = perf['instructions']['counter-value']
    entry['branch_misses'] = perf['branch-misses']['counter-value']
    entry['branch_count'] = perf['branches']['counter-value']
    entry['ins_per_cycle'] = perf['instructions']['metric-value']
    entry['cache_misses_percent'] = perf['cache-misses']['metric-value']

    if args.tool == 'emu':
        with open('profiling.json', 'r', encoding='utf-8') as emuFile:
            emuData = json.load(emuFile)
        entry['instruction_counts'] = emuData['instruction_counts']

    return entry

usesRunner = args.tool in ['emu', 'qemu']
binariesPath = Path(binaryDirectories[args.tool])

print(f'Running benchmark for {args.tool} using binaries in {binariesPath}, using runner: {usesRunner}')

results = {
    'runs' : [],
    'info' : {}
}

for binaryName in sorted(binariesPath.iterdir()):
    if not binaryName.is_file():
        continue

    command = ['perf', 'stat', '-j', '-o', 'result.json', '-e', ','.join(perfArgs)]

    if usesRunner:
        command.append(runnerPaths[args.tool])

    command.append(str(binaryName))

    if binaryName.name == 'coremark':
        command.extend(['0x0', '0x0', '0x66', '10'])

    completedProcess = subprocess.run(command, check=True, capture_output=True, text=True)

    # Extract benchmarks reported runtime
    extractedValue = extractBenchmarkRuntime(completedProcess.stdout)

    print(f'{binaryName.name}: {extractedValue}')

    # Fix dumb perf output json
    perfOutput = loadPerfOutputJson('result.json')
    data = processRun(binaryName.name, perfOutput, extractedValue, command)
    results['runs'].append(data)