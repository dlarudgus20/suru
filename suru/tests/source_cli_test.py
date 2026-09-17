"""Source CLI and REPL regressions using only the Python standard library."""

import pathlib
import subprocess
import sys
import tempfile
import unittest


CLI = pathlib.Path(sys.argv.pop(1)).resolve()


class SourceCliTest(unittest.TestCase):
    def run_cli(self, *arguments, input_text=None):
        return subprocess.run(
            [CLI, *arguments], input=input_text, capture_output=True, text=True
        )

    def test_file_execute_ast_and_ir(self):
        with tempfile.TemporaryDirectory(prefix="suru-source-") as directory:
            root = pathlib.Path(directory)
            source = root / "sample.suru"
            image = root / "sample.sbc"
            yaml = root / "sample.yaml"
            source.write_text("print(40 + 2)\n", encoding="utf-8")

            executed = self.run_cli(source)
            self.assertEqual(executed.returncode, 0, executed.stderr)
            self.assertIn("42.000000", executed.stdout)

            ast = self.run_cli("--ast", source, "-o", yaml)
            self.assertEqual(ast.returncode, 0, ast.stderr)
            self.assertIn("kind: 'Block'", yaml.read_text(encoding="utf-8"))

            emitted = self.run_cli("--sbc", source, "-o", image)
            self.assertEqual(emitted.returncode, 0, emitted.stderr)
            self.assertEqual(image.read_bytes()[:4], b"\x00SBC")
            loaded = self.run_cli(image)
            self.assertEqual(loaded.returncode, 0, loaded.stderr)
            self.assertEqual(loaded.stdout, executed.stdout)

    def test_explicit_batch_stdin(self):
        result = self.run_cli("-", input_text="print(6 * 7)\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("42.000000", result.stdout)

    def test_ast_and_ir_repls(self):
        ast = self.run_cli("--ast", input_text="return 1\n.exit\n")
        self.assertEqual(ast.returncode, 0, ast.stderr)
        self.assertIn("kind: 'Block'", ast.stdout)
        self.assertIn("> ", ast.stderr)

        ir = self.run_cli("--ir", input_text="return 1\n.exit\n")
        self.assertEqual(ir.returncode, 0, ir.stderr)
        self.assertIn(".chunk <main>", ir.stdout)
        self.assertIn("RETURN", ir.stdout)
        self.assertIn("> ", ir.stderr)
        self.assertFalse(any(line.startswith("> ") or line.startswith(">> ")
                             for line in ir.stdout.splitlines()))
        self.assertIn("range:", ast.stdout)
        self.assertIn("begin:", ast.stdout)
        self.assertIn("end:", ast.stdout)

    def test_sbc_rejects_binary_stdout_before_reading_input(self):
        for source in ("-", "nonexistent-source.suru"):
            result = self.run_cli("--sbc", source, input_text="invalid source")
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, "")
            self.assertIn("requires -o", result.stderr)

    def test_sbc_stdin_binary_file(self):
        with tempfile.TemporaryDirectory(prefix="suru-source-") as directory:
            image = pathlib.Path(directory) / "stdin.sbc"
            result = self.run_cli("--sbc", "-", "-o", image,
                                  input_text="print(10)\n")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertEqual(image.read_bytes()[:4], b"\x00SBC")
            loaded = self.run_cli(image)
            self.assertEqual(loaded.returncode, 0, loaded.stderr)
            self.assertIn("10.000000", loaded.stdout)

    def test_execute_repl_preserves_globals(self):
        result = self.run_cli(input_text="x = 40\nx = x + 2\nprint(x)\n.exit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("42.000000", result.stdout)

    def test_repl_rejects_output_option(self):
        result = self.run_cli("--ast", "-o", "unused.yaml")
        self.assertEqual(result.returncode, 2)
        self.assertIn("not supported in REPL mode", result.stderr)

    def test_input_formats_outputs_and_roundtrip(self):
        with tempfile.TemporaryDirectory(prefix="suru-formats-") as directory:
            root = pathlib.Path(directory)
            source = root / "source.suru"
            assembly = root / "assembly.sura"
            binary = root / "binary.sbc"
            source.write_text("local fn f() end local fn f() end print(42)\n", encoding="utf-8")
            ir = self.run_cli("--ir", source)
            self.assertEqual(ir.returncode, 0, ir.stderr)
            self.assertIn(".chunk <main>", ir.stdout)
            saved = self.run_cli("--ir", source, "-o", assembly)
            self.assertEqual(saved.returncode, 0, saved.stderr)
            self.assertEqual(saved.stdout, "")
            self.assertEqual(assembly.read_text(encoding="utf-8"), ir.stdout)

            for input_format, input_path in (("src", source), ("ir", assembly)):
                with self.subTest(input_format=input_format):
                    executed = self.run_cli(f"--in={input_format}", input_path)
                    self.assertEqual(executed.returncode, 0, executed.stderr)
                    self.assertIn("42.000000", executed.stdout)
                    normalized = self.run_cli(f"--in={input_format}", "--ir", input_path)
                    self.assertEqual(normalized.returncode, 0, normalized.stderr)
                    self.assertEqual(normalized.stdout, ir.stdout)
                    emitted = self.run_cli(f"--in={input_format}", "--sbc", input_path, "-o", binary)
                    self.assertEqual(emitted.returncode, 0, emitted.stderr)
                    self.assertEqual(emitted.stdout, "")
                    self.assertEqual(binary.read_bytes()[:4], b"\x00SBC")

            loaded = self.run_cli("--in=sbc", binary)
            self.assertEqual(loaded.returncode, 0, loaded.stderr)
            self.assertIn("42.000000", loaded.stdout)
            disassembled = self.run_cli("--disas", binary)
            self.assertEqual(disassembled.returncode, 0, disassembled.stderr)
            self.assertEqual(disassembled.stdout, ir.stdout)
            disassembly_file = root / "disassembly.sura"
            result = self.run_cli("--in=sbc", "--disas", binary, "-o", disassembly_file)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertEqual(disassembly_file.read_text(encoding="utf-8"), ir.stdout)
            roundtrip = root / "roundtrip.sbc"
            result = self.run_cli("--sbc", disassembly_file, "-o", roundtrip)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(roundtrip.read_bytes(), binary.read_bytes())

            # Content never overrides extension selection; explicit --in does.
            for input_format, data in (("src", source.read_bytes()),
                                       ("ir", assembly.read_bytes()),
                                       ("sbc", binary.read_bytes())):
                neutral = root / f"{input_format}.txt"
                neutral.write_bytes(data)
                explicit = self.run_cli(f"--in={input_format}", neutral)
                self.assertEqual(explicit.returncode, 0, explicit.stderr)
                self.assertIn("42.000000", explicit.stdout)
                guessed = self.run_cli(neutral)
                self.assertEqual(guessed.returncode, 0 if input_format == "src" else 1)
            for suffix, data, explicit_format in ((".SURA", assembly.read_bytes(), "ir"),
                                                   (".SBC", binary.read_bytes(), "sbc")):
                uppercase = root / ("uppercase" + suffix)
                uppercase.write_bytes(data)
                guessed = self.run_cli(uppercase)
                self.assertEqual(guessed.returncode, 0, guessed.stderr)
                uppercase.write_bytes(source.read_bytes())
                explicit = self.run_cli("--in=src", uppercase)
                self.assertEqual(explicit.returncode, 0, explicit.stderr)
                guessed = self.run_cli(uppercase)
                self.assertEqual(guessed.returncode, 1)
            no_extension = root / "source"
            no_extension.write_bytes(source.read_bytes())
            self.assertEqual(self.run_cli(no_extension).returncode, 0)

    def test_ir_batch_stdin(self):
        assembly = self.run_cli("--ir", "-", input_text="print(42)\n")
        self.assertEqual(assembly.returncode, 0, assembly.stderr)
        executed = self.run_cli("--in=ir", "-", input_text=assembly.stdout)
        self.assertEqual(executed.returncode, 0, executed.stderr)
        self.assertIn("42.000000", executed.stdout)
        normalized = self.run_cli("--in=ir", "--ir", "-", input_text=assembly.stdout)
        self.assertEqual(normalized.returncode, 0, normalized.stderr)
        self.assertEqual(normalized.stdout, assembly.stdout)
        with tempfile.TemporaryDirectory(prefix="suru-stdin-") as directory:
            image = pathlib.Path(directory) / "stdin.sbc"
            result = self.run_cli("--in=ir", "--sbc", "-", "-o", image,
                                  input_text=assembly.stdout)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(self.run_cli(image).returncode, 0)
        invalid = self.run_cli("--in=ir", "-", input_text=".chunk main 0 0\nINVALID\n")
        self.assertEqual(invalid.returncode, 1)
        self.assertIn("assembly error at line 2:", invalid.stderr)

    def test_invalid_combinations_are_rejected_before_input(self):
        combinations = [
            ("--in=ir", "--ast", "missing"),
            ("--in=sbc", "--ast", "missing"),
            ("--in=sbc", "--ir", "missing"),
            ("--in=sbc", "--sbc", "missing", "-o", "unused.sbc"),
            ("--in=src", "--disas", "missing"),
            ("--in=ir", "--disas", "missing"),
            ("--in=sbc", "-"),
            ("--in=sbc", "--disas", "-"),
            ("--sbc",), ("--sbc", "-o", "unused.sbc"), ("--disas",),
            ("--in=ir",), ("--in=sbc",),
            ("--in=ir", "--ir"), ("--in=ir", "--sbc"),
            ("--in=", "missing"), ("--in=unknown", "missing"),
            ("--in=src", "--in=src", "missing"),
            ("missing", "-o", "unused"),
            ("--ast", "--ast", "missing"),
            ("--ir", "-o", "unused", "-o", "unused", "missing"),
            ("--disassemble", "missing"),
        ]
        modes = ("--ast", "--ir", "--sbc", "--disas")
        combinations.extend((first, second, "missing")
                            for first in modes for second in modes)
        for arguments in combinations:
            with self.subTest(arguments=arguments):
                result = self.run_cli(*arguments, input_text="invalid source")
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertNotIn("failed to open input", result.stderr)
                self.assertNotIn("> ", result.stderr)

    def test_multiline_strings_continue_in_repl(self):
        for quote in ("'", '"'):
            for mode in ((), ("--ast",), ("--ir",)):
                with self.subTest(quote=quote, mode=mode):
                    result = self.run_cli(*mode,
                        input_text=f"print({quote}hello\nworld{quote})\nprint(42)\n.exit\n")
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn(">> ", result.stderr)
                    self.assertNotIn("error:", result.stderr)
                    self.assertFalse(any(line.startswith("> ") or line.startswith(">> ")
                                         for line in result.stdout.splitlines()))
                    if not mode:
                        self.assertIn("hello\nworld", result.stdout)
                        self.assertIn("42.000000", result.stdout)
                    elif mode == ("--ast",):
                        self.assertIn("kind: 'StringLiteral'", result.stdout)
                        self.assertIn("end: { line: 2, column: 7 }", result.stdout)
                    else:
                        self.assertIn(".chunk <main>", result.stdout)
        batch = self.run_cli("-", input_text='print("hello\n')
        self.assertEqual(batch.returncode, 1)
        self.assertEqual(batch.stdout, "")
        self.assertIn("unterminated string", batch.stderr)


if __name__ == "__main__":
    unittest.main()
