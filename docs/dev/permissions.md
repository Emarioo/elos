Hello, I try to design security into the OS but I don't know if it truly is secure.

# ELOS Permissions

## Executable identity

Each executable is identified by its **SHA-256 hash**.

```text
Executable → SHA-256 → permission grants
```

Permission grants normally apply to the exact executable hash. If the executable changes, its hash changes and permissions must be granted again.


## Permissions

Permissions describe what an application may access.

For example:

```text
ELOS_PERM_NETWORK
```

can contain a list of allowed endpoints:

```text
127.0.0.1:9070
192.168.1.0/24:5000
8.8.8.8:53
```

There should not be application-specific permissions such as `ELOS_PERM_PRISM`. PRISM is accessed by granting network permission to its endpoint.

The kernel knows which process made a connection, so services do not need to trust an application-supplied PID or identity.

## Granting permissions

Applications request permissions through the OS.

The permission UI shows:

```
┌─────────────────────────────────────────────┐
│ Super Game                                  │
│                                             │
│ /home/user/downloads/super_game.elf         │
│ SHA-256: Af8921Af18592                      │
|                                             │
| Publisher: Foo Software                     │
│ Version:   1.2.3                            │
│ Verified by https://elos-exe-database.org   │
│                                             │
│ Requested permissions:                      │
│                                             │
│  [X] PRISM                                  │
│  [ ] Network                                │
│  [X] Audio                                  │
│  [X] Files                                  │
│                                             │
│ [ Allow selected ]                          │
└─────────────────────────────────────────────┘
```


Several permissions can be requested together. The application may wait for the user's decision, or request them asynchronously using the ELOS async API.

A denied permission will be implicitly denied by future requests.
You can edit permission settings to change it.

A granted permission cannot be removed while application is running (it would be complicated to implement).


## Developer permissions

Hash-based permissions should not make development inconvenient.

A developer can explicitly trust a directory:

```text
/home/user/dev/mygame/
```

and grant permissions to executables run from that directory.

For example:

```text
Developer permissions

Path:
    /home/user/dev/mygame/

[x] Allow all permissions I have
```

This allows rebuilt executables to continue working without requiring a new permission grant for every changed SHA-256 hash.

## Publisher verification

This is an optional UX feature. ELOS would automatically verify the executable hash from the publisher.
Convenient for popular programs.

The section `.note-publisher` would contain a publisher identity (name, URL).
The OS asks an endpoint at `https://elos-exe-database.org` if the publisher has registered the exe hash.
We can provide a list of trusted authorities to check.

There are some security stuff to consider:
- A malicious user now has a database where they can try to reproduce an exe hash. SHA-256 should be okay.
- The exe can provide a misleading publisher identity and register a malicious exe hash. If the database
administrators are restrictive with the publisher identity then maybe ok.

An offline system can therefore display below and still allow the user to grant permissions.

```text
Application: Super Awesome Game
SHA-256:     A83F...

Publisher:
    Super Awesome Games
    ? Identity registered
    ? Hash confirmed by publisher

Version:
    1.2.3
```

The code to check/verify the identity should be a user process, not kernel code.

The publisher note section can't provide it's own database address to check against since it can maliciously marking all exe hashes as okay.

Publisher verification is just an idea to consider.
