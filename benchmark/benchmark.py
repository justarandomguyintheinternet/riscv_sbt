import argparse
import json
import re
import subprocess
from pathlib import Path
import datetime
from numbers import Number
import statistics

binaryDirectories = {
    'native': 'bin/x86',
    'sbt': 'translated',
    'emu': 'bin/riscv',
    'qemu': 'bin/riscv'
}

runnerPaths = {
    'qemu': 'qemu-riscv32-static',
    'emu': '../cmake-build-release-wsl/emulator/emulator'
}

perfArgs = [ 'task-clock', 'instructions', 'branch-misses', 'branches', 'cpu-cycles', 'cache-references', 'cache-misses' ]

parser = argparse.ArgumentParser(description='Benchmark runner')
parser.add_argument('--tool', action="store", dest='tool', default='native', help='Tool which to benchmark against.', choices=['native', 'sbt', 'emu', 'qemu'])
parser.add_argument('--name', action="store", dest='name', default='run', help='Name of the benchmarking run, used for filename.')
parser.add_argument('--runs', action="store", dest='num_runs', type=int, default=3, help='Number of benchmark runs to execute.')
args = parser.parse_args()

def extractBenchmarkRuntime(outputText):
    outputText = outputText.strip()

    if not outputText:
        return None

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
    entry['branch_misses_percent'] = perf['branch-misses']['metric-value']

    if args.tool == 'emu':
        with open('profiling.json', 'r', encoding='utf-8') as emuFile:
            emuData = json.load(emuFile)
        entry['instruction_counts'] = emuData['instruction_counts']
        entry['register_accesses'] = emuData['register_accesses']

    return entry

def medianValues(values):
    if not values:
        return None

    firstValue = values[0]

    if isinstance(firstValue, dict):
        medianed = {}
        for key in firstValue:
            medianed[key] = medianValues([value[key] for value in values])
        return medianed

    if isinstance(firstValue, list):
        medianed = []
        for index in range(len(firstValue)):
            medianed.append(medianValues([value[index] for value in values]))
        return medianed

    if isinstance(firstValue, Number) and not isinstance(firstValue, bool):
        return statistics.median(values)

    return firstValue

def medianRuns(runs):
    if not runs:
        return { 'binaries': [] }

    medianBinaries = []
    for binaryIndex in range(len(runs[0]['binaries'])):
        binaryRuns = [run['binaries'][binaryIndex] for run in runs]
        medianBinaries.append(medianValues(binaryRuns))

    return {
        'binaries': medianBinaries
    }

usesRunner = args.tool in ['emu', 'qemu']
binariesPath = Path(binaryDirectories[args.tool])

print(f'Running benchmark for {args.tool} using binaries in {binariesPath}, using runner: {usesRunner}')

results = {
    'runs' : [],
    'median' : {},
    'info' : {}
}

for runIndex in range(args.num_runs):
    runData = {
        'run': runIndex + 1,
        'binaries': []
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

        print(f'Run {runIndex + 1}/{args.num_runs} - {binaryName.name}: {extractedValue}')

        # Fix dumb perf output json
        perfOutput = loadPerfOutputJson('result.json')
        data = processRun(binaryName.name, perfOutput, extractedValue, command)
        runData['binaries'].append(data)

    results['runs'].append(runData)

results['median'] = medianRuns(results['runs'])

results['info'] = {
    'tool': args.tool,
    'num_runs': args.num_runs,
    'timestamp': datetime.datetime.now()
}

with open(f'./benchmarkResults/{args.name}_results.json', 'w') as f:
    f.write(json.dumps(results, indent=4, sort_keys=True, default=str))
