# Stack sampler (ptrace attach is blocked here, so the game runs under gdb).
# Run through tools/perf/sample.sh, which sends SIGSTOP for each sample.
set pagination off
set confirm off
handle SIGSTOP stop print nopass
handle SIGSEGV SIGBUS SIGUSR1 SIGUSR2 SIGPIPE SIG32 SIG33 SIG34 SIG35 SIG36 SIG37 SIG38 SIGINT nostop noprint pass
run
source tools/perf/sample.py
