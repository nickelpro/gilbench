from itertools import repeat
import threading
import balmbench

data = balmbench.read_file("mobydick.bin")


def balm(loops=1000):
  for _ in repeat(None, loops):
    balmbench.bench_balm(data)


def cpy(loops=1000):
  for _ in repeat(None, loops):
    balmbench.bench_cpy(data)


def setup_threads(f):
  threads = []
  for _ in repeat(None, 8):
    threads.append(threading.Thread(target=lambda: f(62)))
    threads.append(threading.Thread(target=lambda: f(63)))
  return threads


def threaded(threads):
  for thread in threads:
    thread.start()
  for thread in threads:
    thread.join()


__benchmarks__ = [
    (cpy, balm, "Use balm strings instead of regular PyUnicode objects"),
]
