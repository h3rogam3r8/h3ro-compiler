#!/usr/bin/env python3
"""Rewrites the roadmap checkboxes in the README from the test results.
A box is ticked when the test suite that proves it exists and passes. So
the README can't claim something works unless there are green tests for it. 
Point it at the MLIR build with --build build-mlir to get the dialect boxes
as well, otherwise those stay unticked.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from xml.etree import ElementTree

# Things with no test suite behind them, like writing the spec. Nothing to run
DONE = "__done__"

# Each phase is a title and a list of (label, what proves it). The third
# option is None for stuff I haven't started.
ROADMAP = [
    ("Foundations", [
        ("Language spec", DONE),
        ("CMake, Ninja, googletest", DONE),
        ("GitHub Actions on linux and macos", DONE),
        ("Address and undefined behaviour sanitizer job", DONE),
    ]),
    ("Front end", [
        ("Lexer with source locations", "Lexer."),
        ("Recursive descent parser and AST", "Parser."),
        ("AST printer, heroc --emit=ast", "ASTPrinter."),
        ("Diagnostics with the source line and a caret", "Diagnostics."),
        ("Negative tests for input that should be rejected", "ParserErrors."),
    ]),
    ("Types and shapes", [
        ("Scopes and name resolution", "Sema."),
        ("dtype rules, no implicit conversion", "SemaTypes."),
        ("Broadcasting", "SemaShapes."),
        ("Symbolic dimensions and matmul checking", "SemaSymbolic."),
    ]),
    ("The hero MLIR dialect", [
        ("Ops and types in TableGen", "mlir/Dialect/Hero"),
        ("AST lowered to IR", None),
        ("hero-opt with a pass registry", None),
    ]),
    ("Graph optimization", [
        ("Canonicalization and constant folding", None),
        ("A dataflow framework I write myself instead of importing", None),
        ("Layout assignment", None),
        ("Operator fusion", None),
    ]),
    ("Down to loops", [
        ("Lowering to linalg, tiling", None),
        ("Bufferization", None),
        ("Memory planning by graph coloring", None),
    ]),
    ("CPU backend", [
        ("Vectorization", None),
        ("JIT through LLVM", None),
        ("Benchmark against OpenBLAS", None),
    ]),
    ("Autotuning", [
        ("Schedule search space", None),
        ("Cost model", None),
        ("Cache what wins", None),
    ]),
    ("GPU backend", [
        ("PTX generation, checked as text so it doesn't need a GPU", None),
        ("Shared memory tiling", None),
        ("Tensor cores", None),
        ("Benchmark against cuBLAS and Triton", None),
    ]),
    ("Real models", [
        ("ONNX importer", None),
        ("StableHLO importer", None),
        ("torch.compile backend", None),
        ("GPT-2 small and ResNet-18", None),
    ]),
    ("Anvil", [
        ("Design the ISA", None),
        ("Instruction selection", None),
        ("Register allocation", None),
        ("List scheduling", None),
        ("Assembler and cycle simulator", None),
    ]),
    ("Extras", [
        ("int8 quantization", None),
        ("Fused attention", None),
        ("Autodiff", None),
    ]),
]

START = "<!-- roadmap:start -->"
END = "<!-- roadmap:end -->"


def test_results(build_dir):
    """Every ctest test name mapped to whether it passed. ctest covers both the googletest ones, 
    which come out as Suite.Name, and the MLIR ones, which are named mlir/path/to/file. Going through
    ctest instead of the gtest binary means one mechanism for both.
    """
    with tempfile.TemporaryDirectory() as tmp:
        report = os.path.join(tmp, "results.xml")
        subprocess.run(["ctest", "--test-dir", build_dir,
                        "--output-junit", report],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       check=False)
        if not os.path.exists(report):
            sys.exit("ctest produced no report, is " + build_dir + " built?")

        tree = ElementTree.parse(report)

    results = {}
    for case in tree.iter("testcase"):
        failed = case.find("failure") is not None or case.get("status") == "fail"
        results[case.get("name")] = not failed
    return results


def prefix_passed(results, prefix):
    """Did anything matching this prefix run, and did all of it pass?"""
    matched = [ok for name, ok in results.items() if name.startswith(prefix)]
    return bool(matched) and all(matched)

def render(suites):
    lines = []
    for number, (title, items) in enumerate(ROADMAP, start=1):
        rendered = []
        done_count = 0

        for label, proof in items:
            if proof is DONE:
                done = True
            elif proof is None:
                done = False
            else:
                done = prefix_passed(suites, proof)

            done_count += done
            rendered.append("  - [%s] %s" % ("x" if done else " ", label))

        whole = "x" if done_count == len(items) else " "
        lines.append("- [%s] **%d. %s**" % (whole, number, title))
        lines.extend(rendered)

    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if the README is out of date instead of "
                         "rewriting it")
    ap.add_argument("--build", default="build",
                    help="a configured and built cmake directory")
    ap.add_argument("--readme", default="README.md")
    args = ap.parse_args()

    if not os.path.isdir(args.build):
        sys.exit("no build directory at " + args.build + ", configure first")

    body = render(test_results(args.build))

    with open(args.readme) as f:
        readme = f.read()

    pattern = re.compile(re.escape(START) + r".*?" + re.escape(END), re.S)
    if not pattern.search(readme):
        sys.exit("could not find the roadmap markers in " + args.readme)

    updated = pattern.sub(START + "\n\n" + body + "\n\n" + END, readme)

    if args.check:
        if updated != readme:
            print("README roadmap is out of date, run scripts/roadmap.py")
            return 1
        print("README roadmap matches the tests")
        return 0

    if updated == readme:
        print("README roadmap already up to date")
        return 0

    with open(args.readme, "w") as f:
        f.write(updated)
    print("README roadmap updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())