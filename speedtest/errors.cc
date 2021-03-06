/*
 * Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "errors.h"

#include <cstdlib>
#include <iostream>

namespace http {

const char *ErrorString(CURLcode error_code) {
  switch (error_code) {
    case CURLE_OK:
      return "CURLE_OK";
    case CURLE_UNSUPPORTED_PROTOCOL:
      return "CURLE_UNSUPPORTED_PROTOCOL";
    case CURLE_FAILED_INIT:
      return "CURLE_FAILED_INIT";
    case CURLE_URL_MALFORMAT:
      return "CURLE_URL_MALFORMAT";
    case CURLE_NOT_BUILT_IN:
      return "CURLE_NOT_BUILT_IN";
    case CURLE_COULDNT_RESOLVE_PROXY:
      return "CURLE_COULDNT_RESOLVE_PROXY";
    case CURLE_COULDNT_RESOLVE_HOST:
      return "CURLE_COULDNT_RESOLVE_HOST";
    case CURLE_COULDNT_CONNECT:
      return "CURLE_COULDNT_CONNECT";
    case CURLE_FTP_WEIRD_SERVER_REPLY:
      return "CURLE_FTP_WEIRD_SERVER_REPLY";
    case CURLE_REMOTE_ACCESS_DENIED:
      return "CURLE_REMOTE_ACCESS_DENIED";
    case CURLE_FTP_ACCEPT_FAILED:
      return "CURLE_FTP_ACCEPT_FAILED";
    case CURLE_FTP_WEIRD_PASS_REPLY:
      return "CURLE_FTP_WEIRD_PASS_REPLY";
    case CURLE_FTP_ACCEPT_TIMEOUT:
      return "CURLE_FTP_ACCEPT_TIMEOUT";
    case CURLE_FTP_WEIRD_PASV_REPLY:
      return "CURLE_FTP_WEIRD_PASV_REPLY";
    case CURLE_FTP_WEIRD_227_FORMAT:
      return "CURLE_FTP_WEIRD_227_FORMAT";
    case CURLE_FTP_CANT_GET_HOST:
      return "CURLE_FTP_CANT_GET_HOST";
    case CURLE_OBSOLETE16:
      return "CURLE_OBSOLETE16";
    case CURLE_FTP_COULDNT_SET_TYPE:
      return "CURLE_FTP_COULDNT_SET_TYPE";
    case CURLE_PARTIAL_FILE:
      return "CURLE_PARTIAL_FILE";
    case CURLE_FTP_COULDNT_RETR_FILE:
      return "CURLE_FTP_COULDNT_RETR_FILE";
    case CURLE_OBSOLETE20:
      return "CURLE_OBSOLETE20";
    case CURLE_QUOTE_ERROR:
      return "CURLE_QUOTE_ERROR";
    case CURLE_HTTP_RETURNED_ERROR:
      return "CURLE_HTTP_RETURNED_ERROR";
    case CURLE_WRITE_ERROR:
      return "CURLE_WRITE_ERROR";
    case CURLE_OBSOLETE24:
      return "CURLE_OBSOLETE24";
    case CURLE_UPLOAD_FAILED:
      return "CURLE_UPLOAD_FAILED";
    case CURLE_READ_ERROR:
      return "CURLE_READ_ERROR";
    case CURLE_OUT_OF_MEMORY:
      return "CURLE_OUT_OF_MEMORY";
    case CURLE_OPERATION_TIMEDOUT:
      return "CURLE_OPERATION_TIMEDOUT";
    case CURLE_OBSOLETE29:
      return "CURLE_OBSOLETE29";
    case CURLE_FTP_PORT_FAILED:
      return "CURLE_FTP_PORT_FAILED";
    case CURLE_FTP_COULDNT_USE_REST:
      return "CURLE_FTP_COULDNT_USE_REST";
    case CURLE_OBSOLETE32:
      return "CURLE_OBSOLETE32";
    case CURLE_RANGE_ERROR:
      return "CURLE_RANGE_ERROR";
    case CURLE_HTTP_POST_ERROR:
      return "CURLE_HTTP_POST_ERROR";
    case CURLE_SSL_CONNECT_ERROR:
      return "CURLE_SSL_CONNECT_ERROR";
    case CURLE_BAD_DOWNLOAD_RESUME:
      return "CURLE_BAD_DOWNLOAD_RESUME";
    case CURLE_FILE_COULDNT_READ_FILE:
      return "CURLE_FILE_COULDNT_READ_FILE";
    case CURLE_LDAP_CANNOT_BIND:
      return "CURLE_LDAP_CANNOT_BIND";
    case CURLE_LDAP_SEARCH_FAILED:
      return "CURLE_LDAP_SEARCH_FAILED";
    case CURLE_OBSOLETE40:
      return "CURLE_OBSOLETE40";
    case CURLE_FUNCTION_NOT_FOUND:
      return "CURLE_FUNCTION_NOT_FOUND";
    case CURLE_ABORTED_BY_CALLBACK:
      return "CURLE_ABORTED_BY_CALLBACK";
    case CURLE_BAD_FUNCTION_ARGUMENT:
      return "CURLE_BAD_FUNCTION_ARGUMENT";
    case CURLE_OBSOLETE44:
      return "CURLE_OBSOLETE44";
    case CURLE_INTERFACE_FAILED:
      return "CURLE_INTERFACE_FAILED";
    case CURLE_OBSOLETE46:
      return "CURLE_OBSOLETE46";
    case CURLE_TOO_MANY_REDIRECTS:
      return "CURLE_TOO_MANY_REDIRECTS";
    case CURLE_UNKNOWN_OPTION:
      return "CURLE_UNKNOWN_OPTION";
    case CURLE_TELNET_OPTION_SYNTAX:
      return "CURLE_TELNET_OPTION_SYNTAX";
    case CURLE_OBSOLETE50:
      return "CURLE_OBSOLETE50";
    case CURLE_PEER_FAILED_VERIFICATION:
      return "CURLE_PEER_FAILED_VERIFICATION";
    case CURLE_GOT_NOTHING:
      return "CURLE_GOT_NOTHING";
    case CURLE_SSL_ENGINE_NOTFOUND:
      return "CURLE_SSL_ENGINE_NOTFOUND";
    case CURLE_SSL_ENGINE_SETFAILED:
      return "CURLE_SSL_ENGINE_SETFAILED";
    case CURLE_SEND_ERROR:
      return "CURLE_SEND_ERROR";
    case CURLE_RECV_ERROR:
      return "CURLE_RECV_ERROR";
    case CURLE_OBSOLETE57:
      return "CURLE_OBSOLETE57";
    case CURLE_SSL_CERTPROBLEM:
      return "CURLE_SSL_CERTPROBLEM";
    case CURLE_SSL_CIPHER:
      return "CURLE_SSL_CIPHER";
    case CURLE_SSL_CACERT:
      return "CURLE_SSL_CACERT";
    case CURLE_BAD_CONTENT_ENCODING:
      return "CURLE_BAD_CONTENT_ENCODING";
    case CURLE_LDAP_INVALID_URL:
      return "CURLE_LDAP_INVALID_URL";
    case CURLE_FILESIZE_EXCEEDED:
      return "CURLE_FILESIZE_EXCEEDED";
    case CURLE_USE_SSL_FAILED:
      return "CURLE_USE_SSL_FAILED";
    case CURLE_SEND_FAIL_REWIND:
      return "CURLE_SEND_FAIL_REWIND";
    case CURLE_SSL_ENGINE_INITFAILED:
      return "CURLE_SSL_ENGINE_INITFAILED";
    case CURLE_LOGIN_DENIED:
      return "CURLE_LOGIN_DENIED";
    case CURLE_TFTP_NOTFOUND:
      return "CURLE_TFTP_NOTFOUND";
    case CURLE_TFTP_PERM:
      return "CURLE_TFTP_PERM";
    case CURLE_REMOTE_DISK_FULL:
      return "CURLE_REMOTE_DISK_FULL";
    case CURLE_TFTP_ILLEGAL:
      return "CURLE_TFTP_ILLEGAL";
    case CURLE_TFTP_UNKNOWNID:
      return "CURLE_TFTP_UNKNOWNID";
    case CURLE_REMOTE_FILE_EXISTS:
      return "CURLE_REMOTE_FILE_EXISTS";
    case CURLE_TFTP_NOSUCHUSER:
      return "CURLE_TFTP_NOSUCHUSER";
    case CURLE_CONV_FAILED:
      return "CURLE_CONV_FAILED";
    case CURLE_CONV_REQD:
      return "CURLE_CONV_REQD";
    case CURLE_SSL_CACERT_BADFILE:
      return "CURLE_SSL_CACERT_BADFILE";
    case CURLE_REMOTE_FILE_NOT_FOUND:
      return "CURLE_REMOTE_FILE_NOT_FOUND";
    case CURLE_SSH:
      return "CURLE_SSH";
    case CURLE_SSL_SHUTDOWN_FAILED:
      return "CURLE_SSL_SHUTDOWN_FAILED";
    case CURLE_AGAIN:
      return "CURLE_AGAIN";
    case CURLE_SSL_CRL_BADFILE:
      return "CURLE_SSL_CRL_BADFILE";
    case CURLE_SSL_ISSUER_ERROR:
      return "CURLE_SSL_ISSUER_ERROR";
    case CURLE_FTP_PRET_FAILED:
      return "CURLE_FTP_PRET_FAILED";
    case CURLE_RTSP_CSEQ_ERROR:
      return "CURLE_RTSP_CSEQ_ERROR";
    case CURLE_RTSP_SESSION_ERROR:
      return "CURLE_RTSP_SESSION_ERROR";
    case CURLE_FTP_BAD_FILE_LIST:
      return "CURLE_FTP_BAD_FILE_LIST";
    case CURLE_CHUNK_FAILED:
      return "CURLE_CHUNK_FAILED";
    case CURLE_NO_CONNECTION_AVAILABLE:
      return "CURLE_NO_CONNECTION_AVAILABLE";
    case CURL_LAST:
      return "CURL_LAST";
    default:
      return "Unknown error";
  }
}

}  // namespace http
