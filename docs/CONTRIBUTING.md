## clang-format and clang-tidy

Standard versions of of clang-format and clang-tidy are used both in local pre-commit checks as well as on the CI pipeline.
The current versions are:
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
