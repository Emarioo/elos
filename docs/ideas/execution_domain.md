(summary of my discussion with ChatGPT)

# Execution Model Overview

This document describes the execution, isolation, and resource model of the OS.
The design prioritizes:
- isolation by default
- explicit authority
- flexible execution
- snapshotting and persistence as first-class concepts

# Binary Artifacts

A Binary Artifact (BA) is the fundamental executable unit.
Always contains code (and possibly data)

May represent:
- an executable
- a library
- a plugin
- runtime-loadable code

Has no inherent permissions, limits, or identity
Is passive and inert until executed
Binary Artifacts are not applications by themselves.

# Applications (Passive)

An application is a collection of files intended to be executed together.

It may be:
- a single Binary Artifact
- a directory containing:
- artifacts
- assets
- configuration
- documentation
- shaders, data, etc.

An application:
- has no limits
- has no storage
- has no authority
- does not declare its own capabilities

Applications are passive objects.

# Execution Domains (ED)

An Execution Domain is the fundamental unit of execution and authority.

An ED defines:
- memory limits
- storage limits
- filesystem namespace
- device and network access
- granted capabilities
- persistence policy

Key properties:

EDs are isolated from each other by default
EDs do not share storage or resource accounting
Resource usage in a child ED does not affect the parent ED
Authority flows from creator to child, explicitly
Applications do not request resources.
Execution Domains impose resources.

# Execution Contexts

Within an Execution Domain:
- programs run as execution contexts
- contexts contain threads and tasks
- multiple programs may run inside the same ED

Execution contexts:
- share the ED’s limits and capabilities
- are lightweight compared to creating a new ED

Default Execution Behavior

Running an application creates a new Execution Domain by default

Sharing an existing domain is always explicit

Example:
```bash
exd arknights_endfield          # new isolated execution domain
exd --same arknights_endfield   # reuse current execution domain
```

Isolation is the default; sharing is intentional.

Declarative Capability Configuration

Execution Domains may be created using declarative capability files.

Example:
```
exd --cap=game.cap arknights_endfield
```

Capability files define:
- storage limits
- memory limits
- filesystem access
- device/network permissions

Imperative overrides are allowed:
```
exd --cap=game.cap --storage=20GB arknights_endfield
```

Rules:
- capability files define defaults
- command-line options override them
- applications do not control limits

Storage Model

Storage belongs to the Execution Domain
Applications see a filesystem provided by the ED
Multiple instances of the same application naturally get separate storage by running in separate EDs
Shared storage is always explicit

This avoids:
- application identity hacks
- directory copying tricks
- name or hash collisions

# Process Visibility

Processes are grouped by Execution Domain.

Example:
```
$ ps
Domain#1
  arknights_endfield
  bash

Domain#2
  vscode
  bash

Domain#3
  webbrowser
```

This reflects actual isolation boundaries and authority relationships.

# Idle Domains

An Execution Domain may exist with:
- no running threads
- no active execution contexts

Idle domains:
- retain memory and storage state
- may be resumed later
- are valid snapshot targets

# Snapshots

Execution Domains can be snapshotted.

A snapshot captures:
- memory state
- execution state
- filesystem state
- granted capabilities
- internal IPC state

Snapshots enable:
- instant resume after shutdown
- game/session continuation without reload
- per-domain suspend/resume
- rollback and recovery

Example:
```
snapshot --domain=1
shutdown
resume domain=1
```

Snapshotting is a natural consequence of ED isolation.

# Design Principles

- Isolation by default
- Explicit sharing
- Declarative authority
- No global application identity
- Execution determines policy, not binaries
- Persistence is an OS feature, not an application trick



what capabilties should execution domain have. storage and memory but then what? What does storage refer to? Amount of file data it can create and rewrite? how to track that? current ED might create a bunch of file and data and when we shut it down and start it again running the same apps the new ED can rewrite a bunch of files again. Also an application can't just call ed_create with capabilities just like that and specify a 100TB and 64GB of storage and RAM. Storage needs to be tied to file system somehow. meaning we need to specify files and directories ED can access. then storage limit has meaning. also, during creation we may have a warning saying limit already reached. then capabilities about devices, mouse, keyboard, raw storage device access, micrphone, camera, monitors, UI, rendering.
