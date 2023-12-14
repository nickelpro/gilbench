from itertools import repeat
import threading
import balmbench

data = balmbench.read_file("mobydick.bin")


def balmblock(loops=10000):
  for _ in repeat(None, loops):
    balmbench.bench_balmblock(data)


def balm(loops=10000):
  for _ in repeat(None, loops):
    balmbench.bench_balm(data)


def cpy(loops=10000):
  for _ in repeat(None, loops):
    balmbench.bench_cpy(data)


def make_pool(f):
  return [threading.Thread(target=lambda: f(625)) for _ in repeat(None, 16)]


def threaded(threads):
  for thread in threads:
    thread.start()
  for thread in threads:
    thread.join()


__benchmarks__ = [
    (cpy, lambda: threaded(balmblock), "cpy vs th_balmblock"),
    (balmblock, balm, "balmblock vs balm"),
    (cpy, balm, "cpy vs balm"),
    (cpy, balmblock, "cpy vs balmblock"),
    (lambda: threaded(cpy), lambda: threaded(balmblock),
     "th_cpy vs th_balmblock"),
]
