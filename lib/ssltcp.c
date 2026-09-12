/*
 * ============================================================================
 *
 *       Filename:  ssltcp.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年01月09日 10时48分49秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jianxi sun (jianxi), ycsunjane@gmail.com
 *   Organization:  
 *
 * ============================================================================
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/x509v3.h>

#include "ssltcp.h"
#include "config.h"
#include "log.h"

SSL_CTX *ctx = NULL;

static const char *
pick_path(const char *envname, const char *defpath)
{
	const char *e = getenv(envname);
	if (e && *e)
		return e;
	return defpath;
}

static void 
ssltcp_cert(SSL_CTX *ctx, const char *file, int type)
{
	int ret;
	ret = SSL_CTX_use_certificate_file(ctx, file, type);
	if(ret != 1) {
		ret = ERR_get_error();
		sys_err("SSL_CTX_use_cert failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		exit(-1);
	}
}

static void 
ssltcp_priv(SSL_CTX *ctx, const char *file, int type)
{
	int ret;
	ret = SSL_CTX_use_PrivateKey_file(ctx, file, type);
	if(ret != 1) {
		ret = ERR_get_error();
		sys_err("SSL_CTX_use_PrivateKey failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		exit(-1);
	}
}

static void
ssltcp_ca(SSL_CTX *ctx, const char *CAfile, const char *CApath)
{
	int ret;
	ret = SSL_CTX_load_verify_locations(ctx, CAfile, CApath);
	if(ret != 1) {
		ret = ERR_get_error();
		sys_err("SSL_ca failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		exit(-1);
	}
}

static void ssltcp_ctx(int isserver)
{
	assert(ctx == NULL);
	int ret;
	const SSL_METHOD *method;
#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L
	if(isserver)
		method = TLS_server_method();
	else
		method = TLS_client_method();
#else
	if(isserver)
		method = SSLv23_server_method();
	else
		method = SSLv23_client_method();
#endif

	ctx = SSL_CTX_new(method);
	if(!ctx) {
		ret = ERR_get_error();
		sys_err("SSL_CTX_new failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		exit(-1);
	}

	/* Disable old/broken protocols and compression. */
	{
		long opts = 0;
#ifdef SSL_OP_NO_SSLv2
		opts |= SSL_OP_NO_SSLv2;
#endif
#ifdef SSL_OP_NO_SSLv3
		opts |= SSL_OP_NO_SSLv3;
#endif
#ifdef SSL_OP_NO_COMPRESSION
		opts |= SSL_OP_NO_COMPRESSION;
#endif
#ifdef SSL_OP_SINGLE_DH_USE
		opts |= SSL_OP_SINGLE_DH_USE;
#endif
#ifdef SSL_OP_SINGLE_ECDH_USE
		opts |= SSL_OP_SINGLE_ECDH_USE;
#endif
		if (opts)
			SSL_CTX_set_options(ctx, opts);
	}
#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L
#ifdef TLS1_2_VERSION
	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
#endif
#endif

	if(isserver) {
		const char *certfile = pick_path("RCTL_CERT_FILE", CERT_FILE);
		const char *privfile = pick_path("RCTL_PRIV_FILE", PRIV_FILE);
		ssltcp_cert(ctx, certfile, SSL_FILETYPE_PEM);
		ssltcp_priv(ctx, privfile, SSL_FILETYPE_PEM);
		if (SSL_CTX_check_private_key(ctx) != 1) {
			ret = ERR_get_error();
			sys_err("SSL cert/key mismatch: %s(%d)\n",
				ERR_error_string(ret, NULL), ret);
			exit(-1);
		}
		/* No client certs are provisioned, so the server
		 * cannot require them. Make that explicit. */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	} else {
		const char *cafile = pick_path("RCTL_CA_FILE", CA_FILE);
		ssltcp_ca(ctx, cafile, NULL);
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
		SSL_CTX_set_verify_depth(ctx, 4);
	}
}

void ssltcp_init(int isserver)
{
#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_load_error_strings();
	SSL_library_init();
#else
	OPENSSL_init_ssl(0, NULL);
#endif
	ssltcp_ctx(isserver);
}

SSL *ssltcp_ssl(int fd)
{
	SSL *ssl = SSL_new(ctx);
	if(!ssl) {
		sys_err("SSL new failed");
		return NULL;
	}

	int ret;
	ret = SSL_set_fd(ssl, fd);
	if(ret != 1) {
		SSL_free(ssl);
		ret = ERR_get_error();
		sys_err("SSL_set_fd failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		return NULL;
	}
	return ssl;
}

int ssltcp_accept(SSL *ssl)
{
	int ret = SSL_accept(ssl);
	if(ret < 0) {
		ret = SSL_get_error(ssl, ret);
		sys_err("SSL_accept failed: %d\n", ret);
		return 0;
	} else if(ret == 0) {
		ret = ERR_get_error();
		sys_err("SSL_accept failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		return 0;
	}
	return 1;
}

static int verify_hostname(X509 *cert, const char *hostname)
{
	unsigned char ipbuf[16];

	if (inet_pton(AF_INET, hostname, ipbuf) == 1)
		return X509_check_ip(cert, ipbuf, 4, 0) == 1;
	if (inet_pton(AF_INET6, hostname, ipbuf) == 1)
		return X509_check_ip(cert, ipbuf, 16, 0) == 1;
	return X509_check_host(cert, hostname, 0, 0, NULL) == 1;
}

int ssltcp_connect(SSL *ssl, const char *hostname)
{
	if (hostname && *hostname) {
		unsigned char ipbuf[16];
		int is_ip = (inet_pton(AF_INET, hostname, ipbuf) == 1 ||
			inet_pton(AF_INET6, hostname, ipbuf) == 1);
		if (!is_ip) {
			/* SNI for DNS names; failure is non-fatal here,
			 * the handshake below will still report errors. */
			SSL_set_tlsext_host_name(ssl, hostname);
		}
	}

	int ret = SSL_connect(ssl);
	if (ret != 1) {
		int err = SSL_get_error(ssl, ret);
		if (err == SSL_ERROR_SYSCALL && ret == 0)
			ret = ERR_get_error();
		else
			ret = err;
		sys_err("SSL_connect failed: %d\n", ret);
		return -1;
	}

	long vr = SSL_get_verify_result(ssl);
	if (vr != X509_V_OK) {
		sys_err("SSL verify failed: %s(%ld)\n",
			X509_verify_cert_error_string(vr), vr);
		return -1;
	}

	if (hostname && *hostname) {
		X509 *peer = SSL_get_peer_certificate(ssl);
		if (!peer) {
			sys_err("SSL verify failed: no peer certificate\n");
			return -1;
		}
		int ok = verify_hostname(peer, hostname);
		if (!ok) {
			char *subj = X509_NAME_oneline(
				X509_get_subject_name(peer), NULL, 0);
			sys_err("SSL hostname verify failed for '%s' peer=%s\n",
				hostname, subj ? subj : "?");
			if (subj)
				OPENSSL_free(subj);
			X509_free(peer);
			return -1;
		}
		X509_free(peer);
	}
	return 0;
}

int ssltcp_read(SSL *ssl, char *buf, int num)
{
	int ret;
repeat:
	ret = SSL_read(ssl, buf, num);
	if(ret < 0) {
		ret = SSL_get_error(ssl, ret);
		if(ret == SSL_ERROR_WANT_READ ||
			ret == SSL_ERROR_WANT_WRITE)
			goto repeat;
		if(ret == SSL_ERROR_ZERO_RETURN) {
			sys_debug("remote ssl closed\n");
			return -1;
		}
		sys_debug("SSL_read failed: %d\n", ret);
		return -1;
	} else if(ret == 0) {
		ret = ERR_get_error();
		sys_debug("SSL_connect failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		return -1;
	}
	return ret;
}

int ssltcp_write(SSL *ssl, char *buf, int num)
{
	int ret;
repeat:
	ret = SSL_write(ssl, buf, num);
	if(ret < 0) {
		ret = SSL_get_error(ssl, ret);
		if(ret == SSL_ERROR_WANT_READ ||
			ret == SSL_ERROR_WANT_WRITE)
			goto repeat;
		if(ret == SSL_ERROR_ZERO_RETURN) {
			sys_debug("remote ssl closed\n");
			return -1;
		}
		sys_debug("SSL_write failed: %d\n", ret);
		return -1;
	} else if(ret == 0) {
		ret = ERR_get_error();
		sys_debug("SSL_connect failed: %s(%d)\n", 
			ERR_error_string(ret, NULL), ret);
		return -1;
	}
	return ret;
}

int ssltcp_shutdown(SSL *ssl)
{
	int ret;
repeat:
	ret = SSL_shutdown(ssl);
	if(ret == 0) {
		goto repeat;
	} else if(ret < 0) {
		ret = SSL_get_error(ssl, ret);
		sys_warn("SSL_shutdown failed: %d\n", ret);
		return -1;
	}
	return 0;
}

void ssltcp_free(SSL *ssl)
{
	SSL_free(ssl);
}
