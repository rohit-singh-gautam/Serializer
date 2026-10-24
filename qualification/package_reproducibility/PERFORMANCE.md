# Startup-inclusive ZIP/OPC measurements

Run the full eleven-SDK reproducibility gate first, then reuse its exact checked
consumer images, generated sources, fixture manifest and provider identities:

```text
python -B qualification/package_process_benchmark.py --baseline out/package-reproducibility/verification.json --compiler out/build/serializer.exe --build out/package-process-baseline
```

This Windows measurement profile executes one producer at a time, takes five fresh
process samples for every SDK and each stored/deflated ZIP/OPC profile, and retains
wall time, process CPU, peak working set, encoded size and generated-source size.
Every timed archive must match the independently inspected canonical baseline and
pass the independent ZIP/XML oracle again. Missing SDKs, changed images/providers,
incomplete inventories and incorrect archives fail the measurement.

Timings include runtime startup, fixture parsing, owning Save, producer validation
and file I/O. Independent archive inspection occurs after the measured process
exits. This is a reproducible startup-inclusive baseline rather than a measurement
of isolated Save throughput. Peak working set is a process residency high-water,
not an allocation count or provider-internal hard limit. The complete functional
gate separately covers forced ZIP64, six inventory permutations and repeated
fresh processes. Shared-machine measurements do not establish isolated regression
thresholds; compare the same workload, SDK and provider policy on an idle host.
