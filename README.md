# wait-free-hashmap
Wait free parallel hashmap implemented in C++.

## Usage

Reference tests for usage examples.

## Style

Use CamelCase for all names. Start types (such as classes, structs, and typedefs) with a capital letter, other names (functions, variables) with a lowercase letter. You may use an all-lowercase name with underscores if your class closely resembles an external construct (e.g., a standard library construct) named that way.

## Testing

When adding new code, please supply tests in the `tests` directory in a file called `Test<feature>.cpp`, utilizing assertions for invariants. Run your tests with `make test`.

## Benchmarking

When adding new code, please supply sequential vs threaded benchmarks in the `benches` directory in a file called `Bench<feature>.cpp`, utilizing the `chrono` namepsace for timing utilities and reporting time in `ms`. Run your benchmarks with `make bench`.

## Data visualization

To spin up our benchmark visualizations, you will need a Conda installation. If you are unfamiliar with Conda, I recommend installing `miniconda`. Once installed, create a new virtual environment with `conda create -n <name>`. Then, you can install the visualization dependencies with `conda install --file analysis/conda_req.txt`. Finally, spin up a Jupyter Labs sessions with `jupyter-lab`, and open and run the `analysis/notebook.ipynb` to view visualizations.
