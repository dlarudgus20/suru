"""Assembler and SBC CLI regressions, using only the Python standard library."""
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

CLI = pathlib.Path(sys.argv.pop(1)).resolve()


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
                self.assertEqual(struct.unpack_from("<I", image_path.read_bytes(), 4)[0], 1)
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

if __name__ == "__main__":
    unittest.main()
