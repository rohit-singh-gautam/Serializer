# Journal record exchange

Run `python example/managed/multilanguage/run.py --compiler <serializer> --feature journal --language <languages>` from the repository root.

Each selected language produces a binary-integer record; every selected language
then consumes every producer, including itself. The receiver modifies only the
qualification revision and preserves every runtime field. Each process also
checks the four ordinary codecs. Inputs include nontrivial metadata and byte arrays.

This is a wire-contract qualification example, **not a native managed runtime**.
