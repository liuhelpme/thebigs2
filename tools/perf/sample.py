# gdb python: on each stop, record the top 14 frames of the wanted threads.
# Env: NSAMP (samples), SAMP_THREADS (comma-separated name prefixes), SAMP_OUT.
import gdb, os
N = int(os.environ.get('NSAMP', '60'))
want = os.environ.get('SAMP_THREADS', 'GPU Commands').split(',')
out = open(os.environ['SAMP_OUT'], 'w')
for i in range(N):
    for t in gdb.selected_inferior().threads():
        name = t.name or ''
        if not any(name.startswith(w) for w in want):
            continue
        t.switch()
        frames = []
        f = gdb.newest_frame()
        while f and len(frames) < 14:
            frames.append(f.name() or hex(f.pc()))
            f = f.older()
        out.write('%d|%s|%s\n' % (i, name, ' < '.join(frames)))
    out.flush()
    gdb.execute('continue')
out.close()
gdb.execute('kill')
