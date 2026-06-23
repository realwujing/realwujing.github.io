# AGENTS.md — KeenTune (keentuned)

## Build

```bash
go mod vendor          # required before first build (vendor dir not committed)
make daemon            # -> pkg/keentuned (daemon binary)
make cli               # -> pkg/keentune  (CLI binary)
make all               # both binaries + fixes python3 path in systemd unit
```

- Build uses `-mod=vendor` with `-extldflags "-static"` — vendor directory **must** exist.
- The daemon binary name is `keentuned`, CLI is `keentune`. Separate binaries, not subcommands of one binary.
- `make install` copies binaries to `/usr/bin/`, configs to `/etc/keentune/keentuned/`, systemd unit to `/lib/systemd/system/`, DBus config to `/etc/dbus-1/system.d/`, and man pages.
- Alternative build: `sh misc/install.sh` — uses `go install` instead of Makefile, also copies configs.

## Architecture

- **IPC**: Daemon exposes methods on DBus **system** bus under name `com.keentune.Service` at object path `/com/keentune/Service/Service`. CLI calls daemon via DBus client in `api/socket.go`. Benchmark/sensitivity results delivered via DBus **session** bus signals.
- **HTTP**: Daemon serves a REST API on configured port (default 9871). Endpoints `/cmd`, `/write`, `/read` are **localhost-only** (auth middleware rejects remote IPs). Endpoints `/benchmark_result`, `/sensitize_result`, `/status` accept remote requests — used by brain/bench/target components.
- **Config**: INI-format at `/etc/keentune/keentuned/keentuned.conf`, loaded with `go-ini`. Sections: `[keentuned]`, `[brain]`, `[target-group-1]`, `[bench-group-1]`.
- **Packages**: `common/` (logging, file ops, HTTP client, config loading), `modules/` (brain, bench, target group, sensitivity, profile, self/job management), `api/` (DBus client), `daemon/` (main server + REST + tests), `cli/` (cobra commands).

## Testing

### Unit tests (Go)
```bash
cd daemon && go test -mod=vendor -v ./...
```
- Uses `gomonkey` for monkey-patching and `goconvey` (Convey/So assertions).
- Only one test file: `daemon/service_test.go`.

### Integration/CLI tests (Python)
```bash
make check              # runs make all -> install -> startup -> python3 test/main.py
# Or directly:
python3 test/main.py    # requires keentuned + brain + target + bench services running
```
- Python `unittest`-based suite. Test discovery in `test/main.py` assembles suites from `CLI_basic/`, `CLI_reliability/`, `MT_restful/`.
- Tests require **all four components** (keentuned on 9871, brain on 9872, target on 9873, bench on 9874) running locally. Full system integration test, not lightweight.
- `make run` restarts keentuned **twice** (Makefile:72 has duplicate `systemctl restart`).

### Running a single CLI test
```bash
cd test && python3 -m unittest CLI_basic.main.RunBasicCase
cd test && python3 -m unittest CLI_reliability.main.RunReliabilityCase
cd test && python3 -m unittest MT_restful.main.RunModelCase
```

## Packaging

- **RPM**: `rpmbuild -ba keentuned.spec` — Go 1.21 static build, produces keentuned-3.1.0 RPM.
- **Companion RPMs** (separate repos): keentune-target (Python 3, noarch), keentune-brain (Python 3, noarch), keentune-bench (Python 3, noarch), keentune-ui (Node.js, noarch).

## Full-stack deployment

```bash
# 1. Build & install keentuned RPM
rpmbuild -ba keentuned.spec && rpm -ivh ~/rpmbuild/RPMS/x86_64/keentuned-*.rpm

# 2. Build & install companion RPMs (each repo has its own .spec)
# keentune-target: python3 setup.py install, keentune-bench: same, keentune-brain: same

# 3. Start services (order matters)
systemctl stop tuned         # keentuned conflicts with tuned
systemctl start keentuned keentune-target keentune-bench
# brain needs detached startup due to tornado ioloop:
cd ~/code/keentune_brain && setsid python3 -c "from brain.brain import main; main()" >/dev/null 2>&1 < /dev/null &

# 3. Start services (order matters)
systemctl stop tuned         # keentuned conflicts with tuned
systemctl start keentuned keentune-target keentune-bench
# brain needs detached startup due to tornado ioloop:
cd ~/code/keentune_brain && setsid python3 -c "from brain.brain import main; main()" >/dev/null 2>&1 < /dev/null &

# 4. Verify
curl -s localhost:9871/status && curl -s localhost:9872/avaliable && curl -s localhost:9873/status && curl -s localhost:9874/status
```

## Gotchas

- If `vendor/` is missing, `go build` fails. Run `go mod vendor` first.
- The daemon's DBus name registration (`com.keentune.Service`) acts as a singleton lock — only one instance can run.
- CLI tests pipe `echo y | keentune ...` for interactive confirmations (see `test/common.py`).
- Config watcher (`fsnotify`) auto-reloads on changes to `keentuned.conf`.
- Work directories created at `/var/keentune/keentuned/` on startup (dump path, tuning/sensitize CSVs, active profile).
- **brain endpoint is `/avaliable`** (misspelling), not `/available`. Detected by keentuned's watchdog.
- **benchmark scripts MUST output `key=value,key=value`** format (comma-separated, no spaces). JSON output will be rejected.
- **kernel 6.6 removed CFS sched params** (`sched_migration_cost_ns`, `sched_min_granularity_ns`, etc.). Filter param JSON by `sysctl -n` before tuning.
- **brain tornado ioloop may exit** when backgrounded with `&`. Use `setsid` to fully detach.
- **ML deps yum/pip conflict**: `yum install python3-scikit-learn python3-xgboost python3-hyperopt` requires `numpy<2`. If pip later installs numpy 2.x, `pip uninstall numpy -y && yum reinstall -y python3-numpy` to fix.