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

            emitted = self.run_cli("--ir", source, "-o", image)
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
        self.assertIn(".chunk main", ir.stdout)
        self.assertIn("RETURN", ir.stdout)

    def test_execute_repl_preserves_globals(self):
        result = self.run_cli(input_text="x = 40\nx = x + 2\nprint(x)\n.exit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("42.000000", result.stdout)

    def test_repl_rejects_output_option(self):
        result = self.run_cli("--ast", "-o", "unused.yaml")
        self.assertEqual(result.returncode, 2)
        self.assertIn("not supported in REPL mode", result.stderr)


if __name__ == "__main__":
    unittest.main()
