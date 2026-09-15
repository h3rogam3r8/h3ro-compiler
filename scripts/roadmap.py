#!/usr/bin/env python3
"""Rewrites the roadmap checkboxes in the README from the test results.
A box is ticked when the test suite that proves it exists and passes. So
the README can't claim something works unless there are green tests for it. 
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

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
        ("Lexer with source locations", "Lexer"),
        ("Recursive descent parser and AST", "Parser"),
        ("AST printer, heroc --emit=ast", "ASTPrinter"),
        ("Diagnostics with the source line and a caret", "Diagnostics"),
        ("Negative tests for input that should be rejected", "ParserErrors"),
    ]),
    ("Types and shapes", [
        ("Scopes and name resolution", "Sema"),
        ("dtype rules, no implicit conversion", "SemaTypes"),
        ("Broadcasting", "SemaShapes"),
        ("Symbolic dimensions and matmul checking", "SemaSymbolic"),
    ]),
    ("The hero MLIR dialect", [
        ("Ops and types in TableGen", None),
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


def passing_suites(test_binary):
    """Suite names that ran with zero failures."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "results.json")
        subprocess.run([test_binary, "--gtest_output=json:" + out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       check=False)
        if not os.path.exists(out):
            sys.exit("could not get results out of " + test_binary)

        with open(out) as f:
            results = json.load(f)

    good = set()
    for suite in results.get("testsuites", []):
        if suite.get("failures", 0) == 0 and suite.get("errors", 0) == 0:
            good.add(suite["name"])
    return good


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
                done = proof in suites

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
    ap.add_argument("--tests", default="build/tests/hero_tests")
    ap.add_argument("--readme", default="README.md")
    args = ap.parse_args()

    if not os.path.exists(args.tests):
        sys.exit("no test binary at " + args.tests + ", build first")

    body = render(passing_suites(args.tests))

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