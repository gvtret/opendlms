# reader_lab

`reader_lab` is a TCP-wrapper client example for the in-tree meter simulator.

Supported production-gate profiles:

- `public`: public association over logical name referencing.
- `reader`: LLS association using the demo reader password.

The `config`/`configurator` profile opens the HLS/Configurator association but
does not execute GET/SET/ACTION until the full HLS pass 3/4 and protected
service path is implemented. Requests in that pending-HLS state are rejected
with a DLMS exception response.

Examples:

```bash
reader_lab public 127.0.0.1 4059 0.0.1.0.0.255 class=8 attr=2
reader_lab reader 127.0.0.1 4059 0.0.1.0.0.255 class=8 attr=2
reader_lab public 127.0.0.1 4059 0.0.1.0.0.255 class=8 attr=2 set-hex=090c07ea061b0c000000ff800000
reader_lab public 127.0.0.1 4059 0.0.1.0.0.255 class=8 action=2
```
