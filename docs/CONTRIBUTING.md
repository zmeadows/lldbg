# Code Quality (clang-format / clang-tidy)

You can use whatever versions of clang-format / clang-tidy you like, or none at all.
However, the CI pipeline will strictly enforce that both format/tidy pass.
The standard versions of of clang-format and clang-tidy for the pipeline are:
 - clang-format: 21.1.2
 - clang-tidy: 21.1.1

There are two primary ways to install these versions:

#### pipx
```
pipx install "clang-format==21.1.2"
pipx install "clang-tidy==21.1.1"
clang-format --version
clang-tidy --version
```

pipx runs the official wheels and keeps them isolated from your Python/site-packages, typically in a location like `~/.local/bin`.

#### venv + pip
```
python3 -m venv .venv && source .venv/bin/activate
python -m pip install "clang-format==21.1.2" "clang-tidy==21.1.1"
```

# Tests

There are curently no unit/integration tests, though hopefully that will change soon.
