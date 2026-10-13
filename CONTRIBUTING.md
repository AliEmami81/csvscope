# Contributing

Please include the smallest CSV that reproduces an issue, the command, expected output, and actual output. Avoid posting private datasets.

Add black-box regressions in `tests/test_cli.py`, compile with warnings enabled, and run the test suite before opening a pull request. Keep the runtime dependency-free and document intentional changes to CSV or numeric parsing.
