import struct

lens = []
for line in open('mobydick.txt').readlines():
  lens.append(len(line))

header = b''
header += len(lens).to_bytes(2, 'little')
for l in lens:
  header += l.to_bytes(2, 'little')

body = open('mobydick.txt', 'rb').read()

with open('mobydick.bin', 'wb') as f:
  f.write(header)
  f.write(body)
