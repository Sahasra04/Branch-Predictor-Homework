# Branch Predictor Homework

Required implementation: `src/my_predictor.h` (`pm_predictor`) with four-way tagged global and bimodal predictor. Optional complete-unit approximation: `cpm_predictor` with loop, BTB, indirect BTB, PIR, fall-through targets.

## Build
`cd src && make`

## Run
`./src/predict traces/164.gzip/gzip.trace.bz2`
`./src/predict_extra_credit traces/164.gzip/gzip.trace.bz2`

## Unit tests
`cd src && g++ -O2 -std=c++11 -o test_predictors test_predictors.cc && ./test_predictors`

## Results
See `benchmark_results.csv` and the single `branch_predictor_report.pdf`. Both executables ran on all 20 traces. The extra-credit unit is an approximation; exact Intel proprietary behavior is not verified.

## Submission
Upload source, this README, and report to GitHub. Attach the repository link and/or project archive in Microsoft Teams as directed by your instructor.
