/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Brian J. Downs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _HTTP_H
#define _HTTP_H

#define HTTP_METHOD_GET                           "GET"
#define HTTP_METHOD_POST                          "POST"
#define HTTP_METHOD_PUT                           "PUT"
#define HTTP_METHOD_DELETE                        "DELETE"
#define HTTP_METHOD_CONNECT                       "CONNECT"
#define HTTP_METHOD_OPTIONS                       "OPTIONS"
#define HTTP_METHOD_TRACE                         "TRACE"

#define HTTP_STATUS_NONE                           0

#define HTTP_STATUS_CONTINUE                       100
#define HTTP_STATUS_SWITCHING_PROTOCOL             101
#define HTTP_STATUS_PROCESSING                     102
#define HTTP_STATUS_EARLY_HINTS                    103

#define HTTP_STATUS_OK                             200
#define HTTP_STATUS_CREATED                        201
#define HTTP_STATUS_ACCEPTED                       202
#define HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION  203
#define HTTP_STATUS_NO_CONTENT                     204
#define HTTP_STATUS_RESET_CONTENT                  205
#define HTTP_STATUS_PARTIAL_CONTENT                206
#define HTTP_STATUS_MULTI_STATUS                   207

#define HTTP_STATUS_MULTIPLE_CHOICES               300
#define HTTP_STATUS_MOVED_PERMANENTLY              301
#define HTTP_STATUS_MOVED_TEMPORARILY              302
#define HTTP_STATUS_SEE_OTHER                      303
#define HTTP_STATUS_NOT_MODIFIED                   304
#define HTTP_STATUS_USE_PROXY                      305
#define HTTP_STATUS_TEMPORARY_REDIRECT             307

#define HTTP_STATUS_BAD_REQUEST                    400
#define HTTP_STATUS_UNAUTHORIZED                   401
#define HTTP_STATUS_PAYMENT_REQUIRED               402
#define HTTP_STATUS_FORBIDDEN                      403
#define HTTP_STATUS_NOT_FOUND                      404
#define HTTP_STATUS_METHOD_NOT_ALLOWED             405
#define HTTP_STATUS_NOT_ACCEPTABLE                 406
#define HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED  407
#define HTTP_STATUS_REQUEST_TIMEOUT                408
#define HTTP_STATUS_CONFLICT                       409
#define HTTP_STATUS_GONE                           410
#define HTTP_STATUS_LENGTH_REQUIRED                411
#define HTTP_STATUS_PRECONDITION_FAILED            412
#define HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE       413
#define HTTP_STATUS_REQUEST_URI_TOO_LONG           414
#define HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE         415
#define HTTP_STATUS_RANGE_NOT_SATISFIABLE          416
#define HTTP_STATUS_EXPECTATION_FAILED             417
#define HTTP_STATUS_UNPROCESSABLE_ENTITY           422
#define HTTP_STATUS_LOCKED                         423
#define HTTP_STATUS_FAILED_DEPENDENCY              424

#define HTTP_STATUS_INTERNAL_SERVER_ERROR          500
#define HTTP_STATUS_NOT_IMPLEMENTED                501
#define HTTP_STATUS_BAD_GATEWAY                    502
#define HTTP_STATUS_SERVICE_UNAVAILABLE            503
#define HTTP_STATUS_GATEWAY_TIMEOUT                504
#define HTTP_STATUS_HTTPVER_NOT_SUPPORTED          505
#define HTTP_STATUS_INSUFFICIENT_STORAGE           507

#endif /* _HTTP_H */
