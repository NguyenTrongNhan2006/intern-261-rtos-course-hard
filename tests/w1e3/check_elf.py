import re
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent.parent.parent
tools = Path("C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin")
for opt in ["O0", "O2"]:
    elf = Path(f"build/w1e3-v2-{opt}/w1e3.elf")
    nm = subprocess.check_output([str(tools/"arm-none-eabi-nm.exe"), "-n", str(elf)], text=True, cwd=root)
    symbols = {m[3]: int(m[1], 16) for line in nm.splitlines()
               if (m := re.fullmatch(r"([0-9a-f]+)\s+([A-Za-z])\s+(\S+)", line))}
    vector = root / f"build/w1e3-v2-{opt}/vector.bin"
    subprocess.run([str(tools/"arm-none-eabi-objcopy.exe"), "-O", "binary", "--only-section=.isr_vector", str(elf), f"build/w1e3-v2-{opt}/vector.bin"], check=True, cwd=root)
    entries = struct.unpack("<" + "I" * (vector.stat().st_size//4), vector.read_bytes())
    assert entries[14] == symbols["PendSV_Handler"] | 1
    assert entries[15] == symbols["SysTick_Handler"] | 1
    for public, native in [("SysTick_Handler", "rta_port_systick_handler"), ("PendSV_Handler", "rta_port_pendsv_handler")]:
        assert symbols[public] != symbols[native] != symbols["Default_Handler"]
        dis = subprocess.check_output([str(tools/"arm-none-eabi-objdump.exe"), "-d", f"--disassemble={public}", str(elf)], text=True, cwd=root)
        assert all(f"<{name}>" in dis for name in [native, "rta_irq_enter", "rta_irq_exit"])
        assert "{r0, lr}" in dis and "bx\tlr" in dis
        assert not re.search(r"\b(?:push|pop).*\br[4-9]\b",dis)
    print(f"PASS {opt}: vector targets profiling wrappers; native ARM_CM3 port distinct; wrapper preserves EXC_RETURN on MSP")
