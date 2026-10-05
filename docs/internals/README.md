# Implementation walkthroughs

This folder explains how Serializer's current code works, using small examples
and links to the implementing functions. Keep proposals in their design guides.

- [Managed stores and transactions](managed_transactions.md): how model_store,
  its nested transaction types, generated editors, and callback forwarding work
  together; includes vertical diagrams and the generated point example.

- [Managed editors](managed_editors.md): shared channels, callback type erasure,
  target resolution, generated getters/setters, and map/array identity checks.

For application setup and public APIs, start with [usage](../usage.md).
