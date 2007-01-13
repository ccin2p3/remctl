/*  $Id$
**
**  Open a connection to a remote server.
**
**  This is the client implementation of opening a connection to a remote
**  server and doing the initial GSS-API negotiation.  This function is shared
**  between the v1 and v2 implementations.  One of the things it establishes
**  is what protocol is being used.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on work by Anton Ushakov
**  Copyright 2002, 2003, 2004, 2005, 2006, 2007
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>
#include <portable/socket.h>

#include <errno.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* Handle compatibility to older versions of MIT Kerberos. */
#ifndef HAVE_GSS_RFC_OIDS
# define GSS_C_NT_USER_NAME gss_nt_user_name
#endif

/* Heimdal provides a nice #define for this. */
#if !HAVE_DECL_GSS_KRB5_MECHANISM
# include <gssapi/gssapi_krb5.h>
# define GSS_KRB5_MECHANISM gss_mech_krb5
#endif

#include <client/internal.h>
#include <client/remctl.h>
#include <util/util.h>


/*
**  Open a new connection to a server.  Returns true on success, false on
**  failure.  On failure, sets the error message appropriately.
*/
int
internal_open(struct remctl *r, const char *host, unsigned short port,
              const char *principal)
{
    struct addrinfo hints, *ai;
    char portbuf[16];
    int fd, status, flags;
    gss_buffer_desc send_tok, recv_tok, name_buffer, *token_ptr;
    gss_buffer_desc empty_token = { 0, (void *) "" };
    gss_name_t name;
    gss_ctx_id_t gss_context;
    OM_uint32 major, minor, init_minor, gss_flags;
    static const OM_uint32 req_gss_flags
        = (GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_CONF_FLAG
           | GSS_C_INTEG_FLAG);

    /* Look up the remote host and open a TCP connection.  Call getaddrinfo
       and network_connect instead of network_connect_host so that we can
       report the complete error on host resolution. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%hu", port);
    status = getaddrinfo(host, portbuf, &hints, &ai);
    if (status != 0) {
        internal_set_error(r, "unknown host %s: %s", host,
                           gai_strerror(status));
        return 0;
    }
    fd = network_connect(ai, NULL);
    freeaddrinfo(ai);
    if (fd < 0) {
        internal_set_error(r, "cannot connect to %s (port %hu): %s", host,
                           port, strerror(errno));
        return 0;
    }
    r->fd = fd;

    /* Import the name into target_name. */
    name_buffer.value = (char *) principal;
    name_buffer.length = strlen(principal) + 1;
    major = gss_import_name(&minor, &name_buffer, GSS_C_NT_USER_NAME, &name);
    if (major != GSS_S_COMPLETE) {
        internal_gssapi_error(r, "parsing name", major, minor);
        close(fd);
        r->fd = -1;
        return 0;
    }

    /* Default to protocol version two, but if some other protocol is already
       set in the remctl struct, don't override.  This facility is used only
       for testing currently. */
    if (r->protocol == 0)
        r->protocol = 2;

    /* Send the initial negotiation token. */
    status = token_send(fd, TOKEN_NOOP | TOKEN_CONTEXT_NEXT | TOKEN_PROTOCOL,
                        &empty_token);
    if (status != TOKEN_OK) {
        internal_token_error(r, "sending initial token", status, major, minor);
        goto fail;
    }

    /* Perform the context-establishment loop.

       On each pass through the loop, token_ptr points to the token to send to
       the server (or GSS_C_NO_BUFFER on the first pass).  Every generated
       token is stored in send_tok which is then transmitted to the server;
       every received token is stored in recv_tok, which token_ptr is then set
       to, to be processed by the next call to gss_init_sec_context.

       GSS-API guarantees that send_tok's length will be non-zero if and only
       if the server is expecting another token from us, and that
       gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if and only if the
       server has another token to send us.

       We start with the assumption that we're going to do protocol v2, but if
       the server ever drops TOKEN_PROTOCOL from the response, we fall back to
       v1. */
    token_ptr = GSS_C_NO_BUFFER;
    gss_context = GSS_C_NO_CONTEXT;
    do {
        major = gss_init_sec_context(&init_minor, GSS_C_NO_CREDENTIAL, 
                    &gss_context, name, (const gss_OID) GSS_KRB5_MECHANISM,
                    req_gss_flags, 0, NULL, token_ptr, NULL, &send_tok,
                    &gss_flags, NULL);

        if (token_ptr != GSS_C_NO_BUFFER)
            gss_release_buffer(&minor, &recv_tok);

        /* If we have anything more to say, send it. */
        if (send_tok.length != 0) {
            flags = TOKEN_CONTEXT;
            if (r->protocol > 1)
                flags |= TOKEN_PROTOCOL;
            status = token_send(fd, flags, &send_tok);
            if (status != TOKEN_OK) {
                internal_token_error(r, "sending token", status, major, minor);
                gss_release_buffer(&minor, &send_tok);
                goto fail;
            }
        }
        gss_release_buffer(&minor, &send_tok);

        /* On error, report the error and abort. */
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED) {
            internal_gssapi_error(r, "initializing context", major,
                                  init_minor);
            goto fail;
        }

        /* If the flags we get back from the server are bad and we're doing
           protocol v2, report an error and abort. */
        if (r->protocol > 1 && (gss_flags & req_gss_flags) != req_gss_flags) {
            internal_set_error(r, "server did not negotiate appropriate"
                               " GSS-API flags");
            goto fail;
        }

        /* If we're still expecting more, retrieve it.  The token size limit
           here is very arbitrary. */
        if (major == GSS_S_CONTINUE_NEEDED) {
            status = token_recv(fd, &flags, &recv_tok, 64 * 1024);
            if (status != TOKEN_OK) {
                internal_token_error(r, "receiving token", status, major,
                                     minor);
                goto fail;
            }
            if (r->protocol > 1 && (flags & TOKEN_PROTOCOL) != TOKEN_PROTOCOL)
                r->protocol = 1;
            token_ptr = &recv_tok;
        }
    } while (major == GSS_S_CONTINUE_NEEDED);

    r->context = gss_context;
    r->ready = 0;
    gss_release_name(&minor, &name);
    return 1;

fail:
    close(fd);
    r->fd = -1;
    gss_release_name(&minor, &name);
    if (gss_context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, gss_context, GSS_C_NO_BUFFER);
    return 0;
}
