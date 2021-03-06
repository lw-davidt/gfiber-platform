#!/usr/bin/python -S

"""Small util functions for /bin/wifi scripts."""

from __future__ import print_function

import collections
import math
import os
import re
import signal
import subprocess
import sys
import time
import unicodedata


_CONFIG_DIR = '/fiber/config/wifi'
FILENAME_KIND = collections.namedtuple(
    'FilenameKind', ('options', 'config', 'pid', 'alive'))(
        options='opts', config='conf', pid='pid', alive='alive')


class Error(Exception):
  """Common base class for all exception types in /bin/wifi."""


class BinWifiException(Error):

  def __init__(self, message, *args):
    super(BinWifiException, self).__init__(message)
    self.args = args

  def __str__(self):
    return '/bin/wifi failed: %s' % (self.message % self.args)


def log(msg, *args, **kwargs):
  print(msg % args, file=sys.stderr, **kwargs)


def atomic_write(filename, data):
  """Performs an atomic file write of data to filename.

  This is done by writing data to filename.new and then renaming filename.new to
  filename.

  Args:
    filename: The filename to to write to.
    data: The data to write.

  Raises:
    BinWifiException: If the write fails.
  """
  tmp_filename = filename + '.new'
  try:
    with open(tmp_filename, 'w') as tmp:
      tmp.write(data)
    os.rename(tmp_filename, filename)
  except (OSError, IOError) as e:
    raise BinWifiException('Writing %s failed: %s', filename, e)


def subprocess_quiet(args, no_stderr=True, no_stdout=False):
  """Run a subprocess command with no stderr, and optionally no stdout."""
  with open(os.devnull, 'w') as devnull:
    kwargs = {}
    if no_stderr:
      kwargs['stderr'] = devnull
    if no_stdout:
      kwargs['stdout'] = devnull
    return subprocess.call(args, **kwargs)


def subprocess_output_or_none(args):
  try:
    return subprocess.check_output(args)
  except subprocess.CalledProcessError:
    return None


def subprocess_lines(args, no_stderr=False):
  """Yields each line in the stdout of a subprocess call."""
  with open(os.devnull, 'w') as devnull:
    kwargs = {}
    if no_stderr:
      kwargs['stderr'] = devnull
    for line in subprocess.check_output(args, **kwargs).split('\n'):
      yield line


def subprocess_line_tokens(args, no_stderr=False):
  return (line.split() for line in subprocess_lines(args, no_stderr))


def babysit(command, name, retry_timeout, pid_filename):
  """Run a command wrapped with babysit and startpid, and piped to logos.

  Args:
    command: The command to run, e.g. ('ls', '-l').
    name: The name to pass to logos.
    retry_timeout: The babysit retry_timeout, in seconds.
    pid_filename: The filename to use for the startpid pid file.

  Returns:
    The name of the interface if found, otherwise None.
  """
  args = ['babysit', str(retry_timeout), 'startpid', pid_filename] + command
  process = subprocess.Popen(args,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  # Sleep for two seconds to give startpid time to create the pid filename.
  time.sleep(2)
  subprocess.Popen(['logos', name], stdin=process.stdout)


def get_mac_address_for_interface(interface):
  with open('/sys/class/net/%s/address' % interface) as mac_address_file:
    return mac_address_file.read().strip()


def increment_mac_address(mac_address):
  numeric_mac_address = int(''.join(mac_address.split(':')), 16) + 1
  numeric_mac_address %= 2 ** 48
  octets = ('%012x' % numeric_mac_address).decode('hex')
  return ':'.join(octet.encode('hex') for octet in octets)


def get_filename(program, kind, disambiguator, tmp=False):
  """Gets the filename for storing a specific kind of state.

  Args:
    program: E.g. 'hostapd' or 'wpa_supplicant'.
    kind: A FILENAME_KIND value.
    disambiguator: E.g. an interface or a band.
    tmp: True if you want the /tmp filename rather than the _CONFIG_DIR one.

  Returns:
    The requested filename.
  """
  return os.path.join('/tmp' if tmp else _CONFIG_DIR,
                      '%s.%s.%s' % (program, kind, disambiguator))


def check_pid(pid_filename):
  """Checks whether a program with a given pid is running.

  Args:
    pid_filename: The location of a file containing the pid to check.

  Returns:
    Whether the program is running.

  Raises:
    BinWifiException: If the pidfile cannot be opened.
  """
  try:
    with open(pid_filename) as pid_file:
      pid = pid_file.read().strip()
    return subprocess_quiet(('kill', '-0', pid)) == 0
  except IOError as e:
    raise BinWifiException("Couldn't open specified pidfile %s: %s",
                           pid_filename, e)


def kill_pid(program, pid_filename):
  """Kill a program which was run with startpid.

  Args:
    program: The program to stop.
    pid_filename: The location of the startpid pid file.

  Returns:
    Whether stopping the program succeeded.
  """
  try:
    subprocess.check_call(('pkillwait', '-f', program))
    subprocess.check_call(('killpid', pid_filename))
  except subprocess.CalledProcessError as e:
    log('Error stopping process: %s', e)
    return False
  finally:
    try:
      os.remove(pid_filename)
    except OSError:
      if os.path.exists(pid_filename):
        raise

  return True


def read_or_empty(filename):
  try:
    with open(filename) as f:
      return f.read().strip()
  except IOError:
    return ''


def validate_set_wifi_options(opt):
  """Validates options to set_wifi.

  Args:
    opt: The options to validate.

  Raises:
    BinWifiException: if anything is not valid.
  """
  band = opt.band
  width = opt.width
  autotype = opt.autotype
  protocols = set(opt.protocols.split('/'))

  if band not in ('2.4', '5'):
    raise BinWifiException('You must specify band with -b2.4 or -b5')

  if (band, width) == ('2.4', '80'):
    raise BinWifiException(
        '80 MHz not valid in 2.4 GHz: type=%s band=%s width=%s',
        autotype, band, width)

  if (band, autotype) == ('2.4', 'DFS'):
    raise BinWifiException('DFS not valid in 2.4 GHz: type=%s band=%s width=%s',
                           autotype, band, width)

  if (band, autotype) == ('5', 'OVERLAP'):
    raise BinWifiException(
        'OVERLAP not allowed in 5 GHz: type=%s band=%s width=%s',
        autotype, band, width)

  if not protocols:
    raise BinWifiException('Must specify some 802.11 protocols')

  for protocol in protocols:
    if protocol not in ('a', 'b', 'ab', 'g', 'n', 'ac'):
      raise BinWifiException('Unknown 802.11 protocol: %s', protocol)

  if width not in ('20', '40', '80'):
    raise BinWifiException('Invalid channel width %s', width)
  elif width == '40' and 'n' not in protocols:
    raise BinWifiException('-p n is needed for 40 MHz channels')
  elif width == '80' and 'ac' not in protocols:
    raise BinWifiException('-p ac is needed for 40 MHz channels')

  if opt.encryption == 'WEP' or '_PSK_' in opt.encryption:
    if 'WIFI_PSK' not in os.environ:
      raise BinWifiException(
          'Encryption enabled; use WIFI_PSK=whatever wifi set ...')

  if opt.wds and not opt.bridge:
    raise BinWifiException('WDS mode enabled; must specify a bridge.')


def sanitize_ssid(ssid):
  """Remove control and non-UTF8 characters from an SSID.

  We use hostapd's utf8_ssid option to specify a UTF8 SSID.

  Args:
    ssid: The SSID to sanitize, as a string.

  Returns:
    The sanitized SSID, as a UTF8-encoded string.
  """
  return ''.join(c for c in ssid.decode('utf-8', 'ignore')
                 if unicodedata.category(c)[0] != 'C').encode('utf-8')


def validate_and_sanitize_bssid(bssid):
  maybe_octets = bssid.lower().split(':')
  if (len(maybe_octets) == 6 and
      all(re.match('[0-9a-f]{2}', maybe_octet)
          for maybe_octet in maybe_octets)):
    return ':'.join(maybe_octets)
  else:
    raise BinWifiException('%s is not a valid BSSID', bssid)


def validate_and_sanitize_psk(psk):
  """Validates a PSK and removes control characters.

  Checks that requirement that PSKs must be either 8-63 ASCII characters or 64
  hex characters.

  Args:
    psk: The PSK to validate, as a string.

  Returns:
    The sanitized PSK, as a string.

  Raises:
    BinWifiException: If the PSK is invalid.
  """
  if len(psk) == 64:
    try:
      psk.decode('hex')
    except TypeError:
      raise BinWifiException('64-character PSK is not hex: %s', psk)
  elif 8 <= len(psk) <= 63:
    try:
      psk.decode('ascii')
    except UnicodeDecodeError:
      raise BinWifiException('8-63 character PSK is not ASCII: %s', psk)
    psk = ''.join(c for c in psk if ord(c) >= 32)
    if len(psk) < 8:
      raise BinWifiException('PSK is not of a valid length: %d', len(psk))
  else:
    raise BinWifiException('PSK is not of a valid length: %d', len(psk))

  return psk


def _lockfile_create_retries(timeout_sec):
  """Invert the lockfile-create --retry option.

  The --retry option specifies how many times to retry.  Each retry takes an
  additional five seconds, starting at 0, so --retry 1 takes 5 seconds,
  --retry 2 takes 15 (5 + 10), and so on.  So:

    timeout_sec = 5 * (retries * (retries + 1)) / 2 =>
    2.5retries^2 + 2.5retries + -timeout_sec = 0 =>
    retries = (-2.5 +/- sqrt(2.5^2 - 4*2.5*-timeout_sec)) / (2*2.5)
    retries = (-2.5 +/- sqrt(6.25 + 10*timeout_sec)) / 5

  We want to ceil this to make sure we have more than enough time, and we can
  even also add 1 to timeout_sec in case we'd otherwise get a whole number and
  don't want it to be close.  We can also reduce the +/- to a + because we
  don't care about negative solutions.

  (Alternatively, we could remove the signal.alarm and
  expose /bin/wifi callers to this logic by letting them specify the retry
  count directly, but that would be even worse than this.)

  Args:
    timeout_sec: The number of seconds the timeout must exceed.

  Returns:
    A value for lockfile-create --retry.
  """
  return math.ceil((-2.5 + math.sqrt(6.25 + 10.0 * (timeout_sec + 1))) / 5.0)


def lock(lockfile, timeout_sec):
  """Attempt to lock lockfile for up to timeout_sec.

  Args:
    lockfile:  The file to lock.
    timeout_sec:  How long to try before giving up.

  Raises:
    BinWifiException:  If the timeout is exceeded.
  """
  def time_out(*_):
    raise BinWifiException('Failed to obtain lock %s after %d seconds',
                           lockfile, timeout_sec)

  retries = _lockfile_create_retries(timeout_sec)

  signal.signal(signal.SIGALRM, time_out)
  signal.alarm(timeout_sec)
  subprocess.call(['lockfile-create', '--use-pid', '--retry', str(retries),
                   lockfile])
  signal.alarm(0)


def unlock(lockfile):
  subprocess.call(['lockfile-remove', lockfile])
