#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
import time


def find_free_port():
    import socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def run_reader(reader, port, *args, profile="public"):
    cmd = [reader, profile, "127.0.0.1", str(port), *args]
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=10)
    if proc.returncode != 0:
        raise AssertionError("reader_lab failed:\n{}\n{}".format(" ".join(cmd), proc.stdout))
    return proc.stdout


def run_reader_fail(reader, port, *args, profile="public"):
    cmd = [reader, profile, "127.0.0.1", str(port), *args]
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=10)
    if proc.returncode == 0:
        raise AssertionError("reader_lab unexpectedly succeeded:\n{}\n{}".format(
            " ".join(cmd), proc.stdout))
    return proc.stdout


def expect(text, needle):
    if needle not in text:
        raise AssertionError("missing {!r} in output:\n{}".format(needle, text))


def main():
    if len(sys.argv) != 3:
        print("usage: live_reader_lab_smoke.py <metersimulator> <reader_lab>", file=sys.stderr)
        return 2

    meter = sys.argv[1]
    reader = sys.argv[2]
    port = find_free_port()
    env = os.environ.copy()
    env["OPENDLMS_METER_PORT"] = str(port)
    meter_log = tempfile.NamedTemporaryFile("w+", delete=False)
    meter_log_name = meter_log.name

    meter_proc = subprocess.Popen([meter, str(port)], stdout=meter_log,
                                  stderr=subprocess.STDOUT, text=True, env=env)
    try:
        time.sleep(0.5)
        if meter_proc.poll() is not None:
            meter_log.seek(0)
            out = meter_log.read()
            raise AssertionError("metersimulator exited before smoke test:\n{}".format(out))

        out = run_reader_fail(reader, port, profile="bogus")
        expect(out, "Bad profile: bogus")

        clock = "0.0.1.0.0.255"
        out = run_reader(reader, port, clock, "class=8", "attr=2")
        expect(out, "Association OK")
        expect(out, "GET OK:")
        expect(out, "access=0")

        out = run_reader(reader, port, clock, "class=8", "attr=2", profile="reader")
        expect(out, "Association OK")
        expect(out, "GET OK:")
        expect(out, "access=0")

        out = run_reader(reader, port, clock, "class=8", "attr=2", "plain", profile="config")
        expect(out, "Configurator: plain HLS")
        expect(out, "Association OK")
        expect(out, "GET OK:")
        expect(out, "access=0")

        new_time = "090c07ea061b0c000000ff800000"
        out = run_reader(reader, port, clock, "class=8", "sap=1", "attr=2",
                         "set-hex={}".format(new_time))
        expect(out, "SET OK:")
        expect(out, "access=0")

        out = run_reader(reader, port, clock, "class=8", "sap=1", "action=2")
        expect(out, "ACTION OK:")
        expect(out, "result=0")

        missing = "0.0.99.9.9.255"
        out = run_reader(reader, port, missing, "class=999", "attr=2")
        expect(out, "GET OK:")
        expect(out, "access=4")

        meter_proc.terminate()
        meter_proc.wait(timeout=3)
        meter_log.flush()
        meter_log.seek(0)
        meter_out = meter_log.read()
        expect(meter_out, "Client disconnected")
    except Exception:
        try:
            meter_proc.terminate()
            meter_proc.wait(timeout=3)
        except Exception:
            pass
        meter_log.flush()
        meter_log.seek(0)
        print("metersimulator output:\n{}".format(meter_log.read()), file=sys.stderr)
        raise
    finally:
        if meter_proc.poll() is None:
            meter_proc.terminate()
            try:
                meter_proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                meter_proc.kill()
                meter_proc.wait(timeout=3)
        meter_log.close()
        try:
            os.unlink(meter_log_name)
        except OSError:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
