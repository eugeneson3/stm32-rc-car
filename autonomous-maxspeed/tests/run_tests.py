"""Requires Python + unicorn, and STM32CubeIDE's Arm GCC tools.
Run: py tests/run_tests.py --tool-bin <directory containing arm-none-eabi-gcc.exe>
"""
import argparse
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
# Optional local installation: py -m pip install --target Debug/test-deps unicorn
sys.path.insert(0, str(root / 'Debug/test-deps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC

parser = argparse.ArgumentParser()
parser.add_argument('--tool-bin', required=True, type=Path)
args = parser.parse_args()
out = root / 'Debug/tests'
out.mkdir(parents=True, exist_ok=True)
def tool(name, *params):
    return subprocess.check_output([str(args.tool_bin / ('arm-none-eabi-' + name + '.exe')),
                                    *map(str, params)], text=True)
tool('gcc', '-mcpu=cortex-m4', '-mthumb', '-O1', '-Wall', '-Wextra', '-Werror',
     '-ffreestanding', '-fno-builtin', '-nostdlib', '-I' + str(root / 'Core/Inc'),
     root / 'tests/firmware_test.c', root / 'Core/Src/navigation.c',
     '-Wl,-Ttext=0x10000,-Tdata=0x20000,-e,tests_main', '-o', out / 'tests.elf')
tool('objcopy', '-O', 'binary', out / 'tests.elf', out / 'tests.bin')
symbols = tool('nm', out / 'tests.elf')
entry = next(int(line.split()[0], 16) for line in symbols.splitlines()
             if line.split()[-1] == 'tests_main')
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x10000, 0x30000)
uc.mem_write(0x10000, (out / 'tests.bin').read_bytes())
uc.mem_map(0x20000000, 0x10000)
uc.reg_write(UC_ARM_REG_SP, 0x2000fff0)
uc.reg_write(UC_ARM_REG_LR, 0x3f001)
uc.emu_start(entry | 1, 0x3f000, count=1000000)
if uc.reg_read(UC_ARM_REG_PC) != 0x3f000:
    raise SystemExit('FAIL: firmware tests did not return within the instruction limit')
line = uc.reg_read(UC_ARM_REG_R0)
if line:
    raise SystemExit(f'FAIL: firmware_test.c:{line}')
print('PASS: navigation, hysteresis, stale/wrap timing, debounce, start/stop, PWM/deadtime, pin mapping, scan and echo timeouts')
