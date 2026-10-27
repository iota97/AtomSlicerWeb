# Thingi 10K Experiments

This folder contains the setup for running experiments on the solids in the Thingi10K dataset and generating a PDF report of the results.

The setup has been tested with the following environment:
- **OS:** Ubuntu 24.04
- **Blender:** 5.2
- **Python:** 3.12.9

## Run the Experiments

First, install the required dependencies:

```bash
pip install -r requirements.txt
```

Next, update `blender_path` in `run.py` to point to your Blender executable.

Then, run the experiments:

```bash
python run.py
```

## Generate the PDF Report

Navigate to the `tex` subdirectory:

```bash
cd tex
```

Run the PDF generation script:

```bash
python generate.py
```

This generates a `latex.tex` file. Compile it using your preferred LaTeX compiler, then compress the resulting PDF using `compress.sh`.