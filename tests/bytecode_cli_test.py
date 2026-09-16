"""Assembler and SBC compatibility regressions, using only the Python standard library."""
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

CLI = pathlib.Path(sys.argv.pop(1)).resolve()


def word(op, a=0, bx=0, v=False):
    return (op << 26) | (int(v) << 25) | (a << 17) | bx


class BytecodeCliTest(unittest.TestCase):
    def run_source(self, source, success=True):
        with tempfile.TemporaryDirectory(prefix="suru-cli-") as directory:
            source_path = pathlib.Path(directory) / "test.sura"
            image_path = pathlib.Path(directory) / "test.sbc"
            source_path.write_text(source, encoding="utf-8")
            result = subprocess.run([CLI, source_path], capture_output=True, text=True)
            self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
            if success:
                emitted = subprocess.run([CLI, "-o", image_path, source_path], capture_output=True, text=True)
                self.assertEqual(emitted.returncode, 0, emitted.stderr)
                self.assertEqual(struct.unpack_from("<I", image_path.read_bytes(), 4)[0], 2)
                loaded = subprocess.run([CLI, image_path], capture_output=True, text=True)
                self.assertEqual(loaded.returncode, 0, loaded.stderr)
                self.assertEqual(loaded.stdout, result.stdout)
            return result

    def test_no_dummy_operands(self):
        self.run_source(".chunk main @va 1\nVARGPREP 0\nVARG.v 0\nRETURN.v 0\n")

    def test_dummy_or_unsupported_open_operands_rejected(self):
        for instruction in ("RETURN.v 0 _", "VARG.v 0 _", "CALL.v 0 _ 0", "LOAD.v 0", "VARGPREP.v 0"):
            with self.subTest(instruction=instruction):
                self.run_source(".chunk main @va 2\n" + instruction + "\n", False)

    def test_arity_syntax_and_limits(self):
        for arity in ("255", "256", "@bad"):
            with self.subTest(arity=arity):
                self.run_source(f".chunk main {arity} 1\nRETURN 0 0\n", False)
        self.run_source(".chunk main @va 0\nRETURN.v 0\n")
        self.run_source(".chunk main 0 0\nRETURN 0 0\n")

    def test_late_and_repeated_prep(self):
        self.run_source(".chunk main @va 1\nLOAD 0 #42\nVARGPREP 0\nVARGPREP 0\nRETURN.v 0\n")

    def test_nonzero_result_contract_and_zero_varargs(self):
        source = '''.const
k_print = string "print"
.chunk main 0 4
GETGLOBALK k_print 0
CLOSURE 1 empty
CALL 1 0 2
CALL 0 2 0
RETURN 0 0
.chunk empty @va 0
VARGPREP 0
VARG.v 0
RETURN.v 0
'''
        self.assertIn("nil\tnil", self.run_source(source).stdout)

    def test_version1_including_fixed_255_and_ignored_i(self):
        # Hand-built v1 fixture: print(7), using i=1 on CALL/RETURN (ignored in v1).
        code = [word(5, 0, 0), word(0, 1, 7, True),
                word(46, 0, (1 << 9), True), word(47, 0, 0, True)]
        for arity, slots in ((0, 2), (255, 255)):
            with self.subTest(arity=arity), tempfile.TemporaryDirectory(prefix="suru-v1-") as directory:
                data = struct.pack("<6I", 0x43425300, 1, 1, 1, len(code), 0)
                data += struct.pack("<BI", 2, 5) + b"print"
                data += struct.pack("<I", 4) + b"main"
                data += struct.pack("<IIBBH", 0, len(code), arity, slots, 0)
                data += struct.pack("<4I", *code)
                path = pathlib.Path(directory) / "legacy.sbc"
                path.write_bytes(data)
                result = subprocess.run([CLI, path], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("7.000000", result.stdout)
                bad_version = bytearray(data)
                struct.pack_into("<I", bad_version, 4, 999)
                path.write_bytes(bad_version)
                result = subprocess.run([CLI, path], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("unsupported sbc version", result.stderr)


if __name__ == "__main__":
    unittest.main()
