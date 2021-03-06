#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Repackage image for signing check.

The image is packaged as follows
Signing will add 16 bytes header (header size and signature offset) plus
8 bytes of padding.

There are two cases:
  if signature_offset == FAKE_SIGN_OFFSET
   header_size contains the length of the file after the 16 byte header
    (or file_size = 16 + header_size)

  if signature_offset != FAKE_SIGN_OFFSET
    header_size =  signature_offset - 16 (not needed/used)
    signature_offset = filesize - 256 - 16 (means the signature is 256 bytes)


The info is free format. Now it is only a string to hold verity table.
|-------------------| <=== Byte 0
| header size       | (integer)
|-------------------| <=== Byte 4
| signature offset  | (integer) pointing to the signature below.
|-------------------| <=== Byte 8
| padding           | Padding
|-------------------| <=== Byte 16
| info (e.g. verity | (4080 bytes)
|  table)           |
|-------------------| <=== Byte 4096
| vmlinuz           | this block is padded to align with 4096 Byte block.
|-------------------| <=== Byte N x 4096
| verity hash table |
|-------------------|
| signature         |
|-------------------|
"""

__author__ = 'kedong@google.com (Ke Dong)'

import os
import re
import shutil
import struct
import subprocess
import sys
import options


optspec = """
repack.py -r <rootfs> -k <kernel> -o <hostdir> -b <bindir> [options...]
--
k,kernel=         kernel image file name [vmlinuz]
r,rootfs=         rootfs image file name [rootfs.squashfs]
o,hostdir=        host directory
b,bindir=         binary directory
s,sign            sign image with production key
bolt_sign         sign a bolt image
t,signing_tool=   tool to call to do the signing [brcm_sign_enc]
q,quiet           suppress print
"""


BLOCK_SIZE = 4096
VERITY_START = '[VERITY-START]'
VERITY_STOP = '[VERITY-STOP]'
VERITY_START_SIZE = '[VERITY-START-SIZE]'
VERITY_STOP_SIZE = '[VERITY-STOP-SIZE]'
FAKE_SIGN_OFFSET = 0x90091efb
quiet = False


def GetRandom():
  """Generate a random hexstring of 64 bytes."""
  return os.urandom(32).encode('hex')


def CheckOutput(args, **kwargs):
  nkwargs = dict(stdout=subprocess.PIPE)
  nkwargs.update(kwargs)
  p = subprocess.Popen(args, **nkwargs)
  data = p.stdout.read()
  retval = p.wait()
  if retval:
    raise Exception('%r returned exit code %d' % (args, retval))
  return data.strip()


def GenerateVerityTable(hostdir, bindir, rootfs):
  """Generate the verity table."""
  return CheckOutput(
      [os.path.join(hostdir, 'usr/bin/verity'), 'mode=create',
       'alg=sha256', 'payload=' + os.path.join(bindir, rootfs),
       'salt=' + GetRandom(), 'hashtree=' + os.path.join(bindir, 'hash.bin')])


def CeilingBlock(size):
  """Calculate the number of blocks based on the size in bytes."""
  return (size + BLOCK_SIZE - 1) / BLOCK_SIZE


def UpdateVerityTable(table, hash_offset):
  """Update the verity table with correct hash table offset.

  Args:
    table:  the verity hash table content
    hash_offset: the hash table offset in sector (each sector has 512 bytes)

  Returns:
    updated verity hash table content
  """
  find_offset = re.compile(r'hashstart=(\d+)')
  return find_offset.sub(r'hashstart=' + str(hash_offset), table)


def FakeSign(fname):
  """Sign the image with valid header but no signature.

  Args:
    fname:  the path to the image file to be signed.
  """
  size = os.stat(fname).st_size
  with open(fname, 'r+b') as f:
    c = f.read()
    f.seek(0)
    f.write(struct.pack('<IIII', size, FAKE_SIGN_OFFSET, 0, 0))
    f.write(c)


def RealSign(bindir, hostdir, key, ifname, ofname, signing_tool):
  """Sign the image with production key."""
  olddir = os.getcwd()
  os.chdir(os.path.join(bindir, 'signing'))
  p = subprocess.Popen([os.path.join(hostdir, 'usr/bin', signing_tool)],
                       stdin=subprocess.PIPE, shell=False)
  for cmd in ['sign_kernel_file', ifname, ofname + '-sig.bin', 'l', key,
              ofname]:
    p.stdin.write(cmd + '\n')
  p.stdin.close()
  retval = p.wait()
  os.chdir(olddir)
  if retval:
    raise Exception('%s returned exit code %d' % (signing_tool, retval))


def RealSignBolt(hostdir, fname):
  """Sign the kernel image with the bolt signing tool.

  This uses the broadcom signing tool to sign a kernel for
  secure boot.  The function expects to be called with the
  current directory equal to out/build/images.

  Args:
    hostdir: Absoluate path to buildroot's out/host directory.
    fname: Name of the image to be signed

  Raises:
    Exception: if the signing tools fails.
  """

  tool_path = os.path.join(hostdir, 'usr/bin/boltsigning/gfiber')
  # Remove any existing file before trying to copy the key over.
  key_path = '/dev/shm/gfhd254_private.pem'
  subprocess.call(['shred', '-fuz', key_path])
  shutil.copy('signing/gfhd254_private.pem', key_path)
  shutil.copy(fname, os.path.join(tool_path, 'kernel.img'))
  exit_code = subprocess.call(
      ['wine', '../imagetool.exe', '-L', 'kernel', '-O', 'kernel.cfg',
       '-K', 'signing=true'], cwd=tool_path)
  subprocess.call(['shred', '-fuz', key_path])
  if exit_code:
    raise Exception('bolt signing tool returned exit code %d' % (exit_code,))
  shutil.copy(os.path.join(tool_path, 'kernel.img.signed'), fname)


def PackVerity(kname, vname, info):
  """Pack verity information in the final image.

  Args:
    kname:  the path to the kernel file.
    vname:  the path to the verity hash table file.
    info:   the original verity table.
  """
  with open(kname, 'r+b') as f:
    verity_data = ''
    kernel_data = f.read()
    kernel_size = len(kernel_data)
    with open(vname, 'rb') as v:
      verity_data = v.read()
    padding_size = (CeilingBlock(kernel_size)*BLOCK_SIZE) - kernel_size
    # offset is the number of sectors to where the hash data begins (see diagram
    # at top of file):
    # The calculates the number of blocks (which are 4096 long) to the hash data
    # start and then multiplies by 8 to convert to sectors which are 512  bytes.
    offset = (1 + CeilingBlock(kernel_size)) * 8
    f.seek(0)
    verity_table = UpdateVerityTable(info, offset)
    if not quiet:
      print verity_table
      print 'hash_size=' + str(len(verity_data))
    f.write('%-4080.4080s' %
            (VERITY_START + verity_table + VERITY_STOP +
             VERITY_START_SIZE + str(len(verity_data)) + VERITY_STOP_SIZE))
    f.write(kernel_data)
    if padding_size > 0:
      f.write('\0'*padding_size)
    f.write(verity_data)


def main():
  global quiet  # gpylint: disable-msg=W0603
  o = options.Options(optspec)
  opt, unused_flags, unused_extra = o.parse(sys.argv[1:])

  quiet = opt.quiet
  verity_table = GenerateVerityTable(opt.hostdir, opt.bindir,
                                     opt.rootfs)
  PackVerity(os.path.join(opt.bindir, opt.kernel),
             os.path.join(opt.bindir, 'hash.bin'),
             verity_table)
  if opt.sign:
    if opt.bolt_sign:
      fname = os.path.join(opt.bindir, opt.kernel)
      RealSignBolt(opt.hostdir, fname)
    else:
      ifname = os.path.join(opt.bindir, 'signing', opt.kernel)
      ofname = os.path.join(opt.bindir, opt.kernel)
      shutil.copy(ofname, ifname)
      RealSign(opt.bindir, opt.hostdir, 'gfiber_private.pem', ifname, ofname,
               opt.signing_tool)
  else:
    FakeSign(os.path.join(opt.bindir, opt.kernel))


if __name__ == '__main__':
  sys.exit(main())
