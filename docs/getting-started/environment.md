# Environment Setup

Wilkins uses [LowFive](https://github.com/diatomic/LowFive) as its data transport layer. LowFive is an HDF5 VOL (Virtual Object Layer) plugin that must be registered with the HDF5 library at runtime. This requires two environment variables to be set before running any Wilkins workflow.

## Required environment variables

### `HDF5_VOL_CONNECTOR`

Tells the HDF5 library to load the LowFive VOL plugin:

```bash
export HDF5_VOL_CONNECTOR="lowfive under_vol=0;under_info={};"
```

This registers LowFive as the active VOL connector, with the native HDF5 VOL as the underlying passthrough layer (`under_vol=0`).

### `HDF5_PLUGIN_PATH`

Tells the HDF5 library where to find the LowFive shared library:

```bash
export HDF5_PLUGIN_PATH=/path/to/lowfive/build/src
```

Set this to the directory containing `liblowfive.so` (Linux) or `liblowfive.dylib` (macOS).

## Spack installations

If you installed Wilkins via Spack, these variables are set automatically when you load the package:

```bash
spack load wilkins
```

No manual setup is needed in this case, since Spack configures the environment automatically.

## Verifying the environment

Wilkins validates the environment at startup. If `HDF5_PLUGIN_PATH` is missing or does not contain the LowFive library, `wilkins-master` will raise an error:

```
RuntimeError: Bad or missing HDF5_PLUGIN_PATH: lowfive library not found.
Set HDF5_PLUGIN_PATH to the directory containing liblowfive.
```

You can also verify manually:

```bash
ls $HDF5_PLUGIN_PATH/liblowfive.*
```

## macOS note

:::{note}
On macOS, Wilkins automatically sets the Python multiprocessing start method to `"fork"` to work around a [known issue](https://github.com/pytorch/pytorch/issues/46648) with the default `"spawn"` method. No user action is needed.
:::

## Summary

| Variable | Purpose | Example |
|---|---|---|
| `HDF5_VOL_CONNECTOR` | Register LowFive VOL plugin | `"lowfive under_vol=0;under_info={};"` |
| `HDF5_PLUGIN_PATH` | Path to `liblowfive` shared library | `/path/to/lowfive/build/src` |
