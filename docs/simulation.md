<!--
SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
SPDX-License-Identifier: CC0-1.0
-->

# Simulation

`seedpick-sim` simulates a user who compares images and measures how well the scheduler finds the top n.
Each image gets a hidden quality drawn from N(0, 1). The simulated answer is noisy, see KONZEPT.md section 3.4.

## Run

```sh
cmake --preset release
cmake --build --preset release --target seedpick-sim
./build/release/bin/seedpick-sim --images 20,50,100,300 --top 1,3,5,10 --runs 100
```

The acceptance test `tst_simulation` runs 100 simulations in the `release` preset.
The `dev` preset uses sanitizers and runs a shorter smoke version.

| Option | Type | Default | Effect |
|---|---|---|---|
| `--images` | list | `20,50,100,300` | Image counts N |
| `--top` | list | `1,3,5,10` | Values of n |
| `--runs` | int | `50` | Runs per configuration |
| `--error-rate` | double | `0.1` | Share of wrong answers on random pairs |
| `--noise` | `logistic`, `flip` | `logistic` | Logistic: errors fall with the quality gap. Flip: fixed error share. |
| `--left-bias` | double | `0` | Logit bias towards the left image |
| `--tau` | double | `1.5` | Prior standard deviation |
| `--min-duels` | int | `2` | Minimum duels per image |
| `--max-repeats` | int | `2` | Decisive results per pair |
| `--threshold` | double | `0.05` | Expected miss share for the stopping criterion |
| `--budget` | double | `2.5` | Soft budget in comparisons per image |
| `--ignore-budget` | flag | off | Continue up to 10 N comparisons |
| `--diagonal` | flag | off | Diagonal uncertainty approximation for all N |
| `--seed` | int | `1` | Base seed |

## Columns

| Column | Meaning |
|---|---|
| mean hit rate | Share of the true top n in the selection |
| expected hit rate | The model's own estimate of the hit rate at the end |
| min hit rate | Worst run |
| perfect | Share of runs with the complete true top n |
| regret | Mean true quality of the true top n minus that of the selection, in standard deviations |
| done before budget | Share of runs that met the stopping criterion before the budget |

## Results

Default settings, logistic noise with 10 % error rate, 100 runs per row.

| N | n | mean hit rate | expected hit rate | min hit rate | perfect | regret | mean comparisons | p90 comparisons | comparisons/N | done before budget | ms/run |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 20 | 1 | 0.700 | 0.646 | 0.00 | 0.70 | 0.083 | 50.0 | 50 | 2.50 | 0.00 | 4 |
| 20 | 3 | 0.873 | 0.800 | 0.33 | 0.65 | 0.025 | 50.0 | 50 | 2.50 | 0.00 | 4 |
| 20 | 5 | 0.852 | 0.814 | 0.60 | 0.32 | 0.039 | 50.0 | 50 | 2.50 | 0.00 | 4 |
| 20 | 10 | 0.903 | 0.830 | 0.80 | 0.19 | 0.035 | 50.0 | 50 | 2.50 | 0.00 | 4 |
| 50 | 1 | 0.770 | 0.651 | 0.00 | 0.77 | 0.071 | 125.0 | 125 | 2.50 | 0.00 | 9 |
| 50 | 3 | 0.847 | 0.804 | 0.33 | 0.56 | 0.027 | 125.0 | 125 | 2.50 | 0.00 | 9 |
| 50 | 5 | 0.858 | 0.822 | 0.40 | 0.42 | 0.030 | 125.0 | 125 | 2.50 | 0.00 | 9 |
| 50 | 10 | 0.871 | 0.829 | 0.70 | 0.16 | 0.034 | 125.0 | 125 | 2.50 | 0.00 | 9 |
| 100 | 1 | 0.710 | 0.676 | 0.00 | 0.71 | 0.073 | 250.0 | 250 | 2.50 | 0.00 | 47 |
| 100 | 3 | 0.863 | 0.779 | 0.33 | 0.61 | 0.025 | 250.0 | 250 | 2.50 | 0.00 | 47 |
| 100 | 5 | 0.870 | 0.831 | 0.40 | 0.42 | 0.019 | 249.9 | 250 | 2.50 | 0.02 | 46 |
| 100 | 10 | 0.859 | 0.854 | 0.70 | 0.13 | 0.028 | 250.0 | 250 | 2.50 | 0.00 | 46 |
| 300 | 1 | 0.750 | 0.702 | 0.00 | 0.75 | 0.038 | 747.8 | 750 | 2.49 | 0.01 | 2485 |
| 300 | 3 | 0.820 | 0.764 | 0.33 | 0.49 | 0.023 | 736.4 | 750 | 2.45 | 0.07 | 2378 |
| 300 | 5 | 0.864 | 0.805 | 0.40 | 0.40 | 0.020 | 745.3 | 750 | 2.48 | 0.04 | 2352 |
| 300 | 10 | 0.873 | 0.865 | 0.60 | 0.19 | 0.022 | 743.5 | 750 | 2.48 | 0.09 | 2478 |

## Known limitations

- With noisy answers the stopping criterion rarely fires. The model's hit rate estimate is conservative, so sessions usually end at the budget.
- For n = 1 the hit rate is 0.70 to 0.77. Allowing 3 decisive repeats per pair raises it to 0.78 at N=100 but lowers n = 5 slightly.
- With the flip noise model (10 % wrong answers regardless of the gap) the hit rate at N=100, n=5 is 0.77 at 2.5 N comparisons.
