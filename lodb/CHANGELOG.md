# Changelog

All notable changes to LoDB will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Minor
- Migrate to LoFS
- `truncate()` method to delete all records from a table while keeping it registered
- `drop()` method to delete all records and unregister a table
- Add diagnostics test suite exercising multiple databases, tables, and all CRUD operations

## [1.2.0] - 2025-12-09

### Minor
- `freeRecords()` helper method to free records returned by `select()`
- `count()` method to count records in a table with optional filtering
- Logo assets with logo.pxd and logo.webp

## [1.1.0] - 2025-12-05

### Patch
- Version bump

## [1.0.2] - 2025-12-05

### Patch
- Updated installation documentation to reflect Meshtastic Plugin Manager usage

## [1.0.0] - 2025-11-28

### Minor
- Protobuf generation is now automatic (no manual `gen_proto.py` script needed)
- Example code demonstrating LoDB usage

## [Initial Release] - 2025-11-06

### Major
- Synchronous, protobuf-based database for Meshtastic
- CRUD operations (Create, Read, Update, Delete)
- SELECT queries with filtering, sorting, and limiting
- Deterministic UUID generation from strings
- Auto-generated UUID support
- Thread-safe filesystem-based storage
- Protocol Buffers integration with nanopb

[Unreleased]: https://github.com/MeshEnvy/lodb/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/MeshEnvy/lodb/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/MeshEnvy/lodb/compare/v1.0.2...v1.1.0
[1.0.2]: https://github.com/MeshEnvy/lodb/compare/v1.0.0...v1.0.2
[1.0.0]: https://github.com/MeshEnvy/lodb/compare/93496d6...v1.0.0
