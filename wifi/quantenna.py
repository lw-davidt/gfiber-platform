#!/usr/bin/python -S

"""Wifi commands for Quantenna using QCSAPI."""

import os
import subprocess
import time

import utils


ALREADY_MEMBER_FMT = ('device %s is already a member of a bridge; '
                      "can't enslave it to bridge %s.")
NOT_MEMBER_FMT = 'device %s is not a slave of %s'


# pylint: disable=protected-access
def _get_interface():
  if not hasattr(_get_interface, '_interface'):
    _get_interface._interface = subprocess.check_output(
        ['get-quantenna-interface']).strip()
  return _get_interface._interface


# pylint: disable=protected-access
def _get_qcsapi():
  # qcsapi_pcie_static runs on PCIe hosts, e.g. GFRG250.
  # call_qcsapi runs on the LHOST, e.g. GFEX250.
  if not hasattr(_get_qcsapi, '_qcsapi'):
    _get_qcsapi._qcsapi = next(
        (qcsapi for qcsapi in ['qcsapi_pcie_static', 'call_qcsapi']
         if utils.subprocess_quiet(['runnable', qcsapi]) == 0), None)
  return _get_qcsapi._qcsapi


def _get_mac_address():
  var = {'wlan0': 'MAC_ADDR_WIFI', 'wlan1': 'MAC_ADDR_WIFI2'}[_get_interface()]
  return subprocess.check_output(['hnvram', '-rq', var]).strip()


def _qcsapi(*args):
  return subprocess.check_output([_get_qcsapi()] + list(args)).strip()


def _brctl(*args):
  return subprocess.check_output(['brctl'] + list(args),
                                 stderr=subprocess.STDOUT).strip()


def _set_interface_in_bridge(bridge, want_in_bridge):
  """Add/remove Quantenna interface from/to the bridge."""
  if want_in_bridge:
    command = 'addif'
    error_fmt = ALREADY_MEMBER_FMT
  else:
    command = 'delif'
    error_fmt = NOT_MEMBER_FMT

  try:
    _brctl(command, bridge, _get_interface())
  except subprocess.CalledProcessError as e:
    if error_fmt % (_get_interface(), bridge) not in e.output:
      raise utils.BinWifiException(e.output)


def _set(mode, opt):
  """Enable wifi."""
  if not _get_interface() or not _get_qcsapi():
    return False

  _qcsapi('rfenable', '0')
  _qcsapi('restore_default_config', 'noreboot')

  config = {
      'bw': opt.width,
      'channel': '149' if opt.channel == 'auto' else opt.channel,
      'mode': mode,
      'pmf': '0',
      'scs': '0',
  }
  for param, value in config.iteritems():
    _qcsapi('update_config_param', 'wifi0', param, value)

  _qcsapi('set_mac_addr', 'wifi0', _get_mac_address())

  if int(_qcsapi('is_startprod_done')):
    _qcsapi('reload_in_mode', 'wifi0', mode)
  else:
    _qcsapi('startprod', 'wifi0')
    for _ in xrange(30):
      if int(_qcsapi('is_startprod_done')):
        break
      time.sleep(1)
    else:
      raise utils.BinWifiException('startprod timed out')

  if mode == 'ap':
    _set_interface_in_bridge(opt.bridge, True)
    _qcsapi('set_ssid', 'wifi0', opt.ssid)
    _qcsapi('set_passphrase', 'wifi0', '0', os.environ['WIFI_PSK'])
    _qcsapi('set_option', 'wifi0', 'ssid_broadcast',
            '0' if opt.hidden_mode else '1')
    _qcsapi('rfenable', '1')
  elif mode == 'sta':
    _set_interface_in_bridge(opt.bridge, False)
    _qcsapi('create_ssid', 'wifi0', opt.ssid)
    _qcsapi('ssid_set_passphrase', 'wifi0', opt.ssid, '0',
            os.environ['WIFI_CLIENT_PSK'])
    # In STA mode, 'rfenable 1' is already done by 'startprod'/'reload_in_mode'.
    # 'apply_security_config' must be called instead.
    _qcsapi('apply_security_config', 'wifi0')

  return True


def _stop(_):
  """Disable wifi."""
  if not _get_interface() or not _get_qcsapi():
    return False

  _qcsapi('rfenable', '0')
  return True


def set_wifi(opt):
  return _set('ap', opt)


def set_client_wifi(opt):
  return _set('sta', opt)


def stop_ap_wifi(opt):
  return _stop(opt)


def stop_client_wifi(opt):
  return _stop(opt)
