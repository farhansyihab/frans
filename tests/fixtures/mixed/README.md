# Mixed-language fixture (Stage 10d)

This directory contains a non-code file on purpose, to verify that
`TreeSitterGraphBuilder::discoverSourceFiles` skips files with
extensions that are not registered as source (`.md` here).

Expected discovery result: 4 source files (`.py`, `.hpp`, `.cpp`, `.ts`),
and this README excluded.
