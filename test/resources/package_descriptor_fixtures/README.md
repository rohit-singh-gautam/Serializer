# Independent ZIP descriptor fixtures

Regenerate these wire fixtures with `python -B test/resources/package_descriptor_fixtures.py`.
The generator authors ZIP fields directly and independently reads all twelve
valid controls through Python's standard `zipfile` implementation.

Each signed/unsigned ZIP32/ZIP64 descriptor form includes an empty payload,
a small model payload, and a payload whose CRC equals the optional descriptor
signature `0x08074b50`. Four additional archives change only the descriptor CRC;
the central directory and compressed payload retain their correct CRC.

C++ and generated Python consume these same bytes. The small ZIP64 fixtures
qualify descriptor width, not the separate large-offset/count ZIP64 gate.
