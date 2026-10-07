"""Read-only inspection of selected render-allocation functions in a running game.

Requires pefile and capstone. Does not write process memory or game files.
Printed disassembly is local diagnostic data, not a redistribution artifact.
"""
import argparse
import bisect
import ctypes
import struct

import capstone
import pefile

parser = argparse.ArgumentParser()
parser.add_argument("exe")
parser.add_argument("pid", type=int)
parser.add_argument("rvas", nargs="+", type=lambda text: int(text, 16))
parser.add_argument("--limit", type=int, default=180)
parser.add_argument("--view", action="store_true", help="Read the observed CLE renderer/view fields without modifying them")
parser.add_argument("--parameters", action="store_true", help="With --view, inspect command parameter methods and bounded renderer viewport candidates")
parser.add_argument("--leaf", action="store_true", help="Decode up to 128 bytes when a selected leaf has no unwind entry")
parser.add_argument("--span", type=lambda text: int(text, 0), default=0, help="Read a bounded contiguous runtime span, including split unwind fragments (maximum 16384 bytes)")
args = parser.parse_args()
if args.span < 0 or args.span > 16384:
    parser.error("--span must be between 0 and 16384")
pe = pefile.PE(args.exe, fast_load=False)
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
kernel.OpenProcess.restype = ctypes.c_void_p
kernel.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel.CloseHandle.argtypes = [ctypes.c_void_p]
psapi = ctypes.WinDLL("psapi", use_last_error=True)
psapi.EnumProcessModules.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
process = kernel.OpenProcess(0x1010, False, args.pid)
if not process:
    raise ctypes.WinError(ctypes.get_last_error())
try:
    modules = (ctypes.c_void_p * 1024)()
    needed = ctypes.c_uint32()
    if not psapi.EnumProcessModules(process, modules, ctypes.sizeof(modules), ctypes.byref(needed)):
        raise ctypes.WinError(ctypes.get_last_error())
    base = modules[0]
    def memory(address, size):
        data = ctypes.create_string_buffer(size)
        count = ctypes.c_size_t()
        if not kernel.ReadProcessMemory(process, address, data, size, ctypes.byref(count)) or count.value != size:
            raise ctypes.WinError(ctypes.get_last_error())
        return data.raw
    def pointer(address):
        return struct.unpack("<Q", memory(address, 8))[0]
    if args.view:
        if pe.FILE_HEADER.TimeDateStamp != 1730414432 or pe.OPTIONAL_HEADER.SizeOfImage != 9019392:
            raise ValueError("The view-field layout is only observed on CLE Kuro I 1.1.0")
        renderer = pointer(base + 0x7D9380)
        context = pointer(renderer + 0x38)
        print(f"VIEW context={context:x}, bind_srv={pointer(pointer(context)+0x88)-base:x}")
        if args.parameters:
            for offset in (0xa8,0xb0,0xb8,0xc0,0xc8,0xd0,0x168,0x188):
                print(f"PARAM method slot={offset:x} rva={pointer(pointer(context)+offset)-base:x}")
            # This is a CPU heap search, not a GPU buffer read or a claim of field identity.
            data=memory(renderer,0xfe880)
            pattern=struct.pack("<4f",1920,1080,1/1920,1/1080)
            offset=data.find(pattern)
            while offset!=-1:
                print(f"PARAM candidate renderer+{offset:x} values={struct.unpack('<4f',data[offset:offset+16])}")
                offset=data.find(pattern,offset+1)
        owner = pointer(base + 0x7D0CE8)
        scene = pointer(owner + 0x4F0)
        if args.parameters:
            for offset in range(0,0x60,8):
                print(f"PARAM original scene method slot={offset:x} rva={pointer(base+0x713e30+offset)-base:x}")
            for offset in (0x3e8,0x400,0x428,0x450):
                address=pointer(scene+offset)
                print(f"PARAM scene+{offset:x} object={address:x} size={struct.unpack('<I',memory(address+8,4))[0] if address else 0}")
                if address and offset in (0x3e8,0x400):
                    vtable=pointer(address)
                    for slot in range(0,0x30,8):
                        print(f"PARAM buffer method slot={slot:x} rva={pointer(vtable+slot)-base:x}")
        virtual = pointer(scene)
        print(f"VIEW renderer={renderer:x}, owner={owner:x}, scene={scene:x}, virtual={virtual-base:x}, render={pointer(virtual+0x30)-base:x}")
        signature = 14695981039346656037
        for byte in memory(pointer(virtual + 0x30), 32):
            signature = ((signature ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        print(f"VIEW render_prefix_fnv64={signature:x}")
        normal = pointer(scene + 0x408)
        normal_backend = pointer(normal + 8)
        print(f"VIEW secondary_normal wrapper={normal:x}, backend={normal_backend:x}, format_width_height={struct.unpack('<3I',memory(normal_backend+0x18,12))}, texture={pointer(normal_backend+0x40):x}, srv={pointer(normal_backend+0x48):x}")
        for label, offset in (("output_color", 0x28), ("shared_depth", 0x30)):
            wrapper = pointer(renderer + offset)
            backend = pointer(wrapper + 8)
            values = struct.unpack("<6I", memory(backend + 0x18, 24))
            print(f"VIEW {label} wrapper={wrapper:x}, backend={backend:x}, dimensions_and_flags={values}")
            print(f"VIEW {label} pointer_fields=" + ",".join(f"{field:x}:{pointer(backend+field):x}" for field in range(0x28, 0x90, 8)))
    functions = [(e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION]
    starts = [f[0] for f in functions]
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    for rva in args.rvas:
        if args.span:
            if rva < 0 or rva + args.span > pe.OPTIONAL_HEADER.SizeOfImage:
                raise ValueError("Span is outside the image")
            print(f"SPAN {rva:x}-{rva+args.span:x}")
            for i, instruction in enumerate(decoder.disasm(memory(base+rva, args.span), rva)):
                if i < args.limit:
                    print(f"{instruction.address:x} {instruction.mnemonic} {instruction.op_str}")
            continue
        index = bisect.bisect_right(starts, rva) - 1
        if index < 0 or rva >= functions[index][1]:
            print(f"{rva:x}: no unwind function")
            if args.leaf:
                for instruction in decoder.disasm(memory(base + rva, 128), rva):
                    print(f"{instruction.address:x} {instruction.mnemonic} {instruction.op_str}")
                    if instruction.mnemonic in ("ret", "jmp"):
                        break
            continue
        start, end = functions[index]
        if end - start > 262144:
            raise ValueError("Unexpected function size")
        data = ctypes.create_string_buffer(end - start)
        count = ctypes.c_size_t()
        if not kernel.ReadProcessMemory(process, base + start, data, len(data), ctypes.byref(count)) or count.value != len(data):
            raise ctypes.WinError(ctypes.get_last_error())
        print(f"FUNCTION {start:x}-{end:x}, selected={rva:x}")
        for i, instruction in enumerate(decoder.disasm(data.raw, start)):
            if i < args.limit or abs(instruction.address - rva) < 100:
                print(f"{instruction.address:x} {instruction.mnemonic} {instruction.op_str}")
finally:
    kernel.CloseHandle(process)
