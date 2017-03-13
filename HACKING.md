Hacking
=======

This document should contain everything you need to get up and
running on TSDP development.


Tools Used
----------

We use the following tools to produce software of the highest
quality:

  - [AFL][afl] - American fuzzy lop is an adaptive fuzzer that we
    use to suss out hangs and crashes related to malicious user
    input.  See the section on [Fuzz Testing](#fuzz-testing) for
    more details.
  - [Valgrind][vg] - Valgrind is a tool for memory debugging.  We
    use it to pinpoint issues with memory (mis-)handling and chase
    down memory leaks.


Testing Discipline
------------------

As a monitoring system toolkit, people expect TSDP to function,
even in the face of horribly incorrect input, maddening network
latency and availability, low or exhausted system resources, and
more.  To this end, we follow a strict regimen of testing
comprised of all kinds of different styles and tools.

### Contract Testing

Contract testing is the first line of defense.  These are test
cases written by hand that exercise some component of the system
in isolation, to determine if it properly satisfies its design
contract.  A prime example of this is `t/contract/qname`, which
tests the parsing and comparison routines in the Qualified Names
implementation.

These are not unit tests.

These are not functional tests.

These are not integration tests.

The world of software testing has obliterated any meaning that
these terms once held.  Instead, we call these _contract tests_,
because they make assertions about the behavior of a given
component and verify that those assertions hold.

Contract tests are found in the `t/contract` directory.  The
entire suite can be run via `make check-contract`.  These tests
are included in the baseline `make check` testing target.

### Regression Testing

Regression testing is the next line of defense.  Whenever a
bug is discovered, it is not considered fixed until it has
accompanying test cases to ensure that future modification to
the codebase doesn't re-introduce the aberrant behavior.

In some instances, it makes sense to modify one or more contract
tests to catch the regression.  In others, dedicated test
programs will need to be written.  Those are found in the
`t/regress` directory.  All regression tests can be run via `make
check-regress`.  These tests are included in the baseline `make
check` testing target.

### Performance Testing

Performance testing seeks to determine if the performance
characteristics of the TSDP implementation are adequate.  These
tests are specifically designed to approximate average timings on
specific operations (parsing a Qualified Name, traversing a list
of N clients, etc.).  The real value from these tests comes from
repeat runs on similar hardware, to catch instances of decreased
performance following code changes.

Performance tests are found in the `t/perf` directory, and can be
run via `make check-perf`.  Results are stored in
`t/perf/results`, and the utilities in `t/perf/bin` exist to
analyze changes in performance characteristics between runs.

These tests are not part of the baseline `make check` testing
target, and normally should only be run as part of release
activities, on similar hardware as previous runs.

### Memory Testing

Memory testing involves running specific tasks under supervision
of a tool like [Valgrind][vg] in order to locate memory errors
like uninitialized value usage and out-of-bounds access, and to
detect memory leaks. The former can lead to obscure crashes
and the latter can impact the longevity of a process that uses
TSDP facilities.

Memory tests also use [gremlin][gremlin] to ensure that we are
properly handling errors from the memory allocation subsystem
(malloc and friends).

Memory tests live under `t/mem`.  These tests can execute slowly,
due to the necessary instrumentation for detecting memory errors.
As such, these tests are not included in the standard `make check`
target.  They can be run via `make check-mem`.

### Stress Testing

System resources are limited, and allocation of such things as
file descriptors and memory can often fail at inopportune moments.
Stress tests simulate these failures by overriding allocation
routines with custom implementations that can be set to fail after
a certain number of calls.  The tests then run under a variety of
circumstances to ensure that all components properly handle
resource exhaustion.

Stress tests live under `t/stress`.  They can be run via the `make
check-stress` target.  Due to their long execution times, these
tests are not included in the standard `make check` target.o

### Fuzz Testing

Fuzzing is the process of sending arbitrary input to a program to
determine if it can correctly handle it.  Fuzz testing automates
this technique in an attempt to find obscure inputs that cause
parts of the TSDP implementation to crash or become unresponsive.

For fuzzing, we use [american fuzzy lop][afl], which instruments
the binary under test and then adapts its mutation strategy based
on changes in the execution path of the target binary.  This leads
to a fairly efficient convergence on meaningful tests, and quickly
finds crashes and hangs.

Fuzz test cases are standalone programs that read standard input,
perform operations on it, and then exit without crashing.  We use
this model to test parsing routines for things like qualified
names, as well as proper deserialization of network messages.

Fuzz tests can be found in `t/fuzz`.  Because fuzzing is a time
investment, there is no `make` target for running the fuzz tests.
There is a `make fuzz-tests` target that will build all of the
binaries for fuzzing, but you will need to run the individual
scripts in `t/fuzz` by hand.


[afl]: http://lcamtuf.coredump.cx/afl/
[vg]:  http://valgrind.org/
