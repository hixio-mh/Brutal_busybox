/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config TLS
//config:	bool "tls (debugging)"
//config:	default n

//applet:IF_TLS(APPLET(tls, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TLS) += tls.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_montgomery_reduce.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_mul_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_sqr_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_rsa.o
////kbuild:lib-$(CONFIG_TLS) += tls_ciphers.o
////kbuild:lib-$(CONFIG_TLS) += tls_aes.o
////kbuild:lib-$(CONFIG_TLS) += tls_aes_gcm.o

//usage:#define tls_trivial_usage
//usage:       "HOST[:PORT]"
//usage:#define tls_full_usage "\n\n"

#include "tls.h"

#if 1
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#define RECORD_TYPE_CHANGE_CIPHER_SPEC  20
#define RECORD_TYPE_ALERT               21
#define RECORD_TYPE_HANDSHAKE           22
#define RECORD_TYPE_APPLICATION_DATA    23

#define HANDSHAKE_HELLO_REQUEST         0
#define HANDSHAKE_CLIENT_HELLO          1
#define HANDSHAKE_SERVER_HELLO          2
#define HANDSHAKE_HELLO_VERIFY_REQUEST  3
#define HANDSHAKE_NEW_SESSION_TICKET    4
#define HANDSHAKE_CERTIFICATE           11
#define HANDSHAKE_SERVER_KEY_EXCHANGE   12
#define HANDSHAKE_CERTIFICATE_REQUEST   13
#define HANDSHAKE_SERVER_HELLO_DONE     14
#define HANDSHAKE_CERTIFICATE_VERIFY    15
#define HANDSHAKE_CLIENT_KEY_EXCHANGE   16
#define HANDSHAKE_FINISHED              20

#define SSL_HS_RANDOM_SIZE              32
#define SSL_HS_RSA_PREMASTER_SIZE       48

#define SSL_NULL_WITH_NULL_NULL                 0x0000
#define SSL_RSA_WITH_NULL_MD5                   0x0001
#define SSL_RSA_WITH_NULL_SHA                   0x0002
#define SSL_RSA_WITH_RC4_128_MD5                0x0004
#define SSL_RSA_WITH_RC4_128_SHA                0x0005
#define SSL_RSA_WITH_3DES_EDE_CBC_SHA           0x000A  /* 10 */
#define TLS_RSA_WITH_AES_128_CBC_SHA            0x002F  /* 47 */
#define TLS_RSA_WITH_AES_256_CBC_SHA            0x0035  /* 53 */
#define TLS_EMPTY_RENEGOTIATION_INFO_SCSV       0x00FF

#define TLS_RSA_WITH_IDEA_CBC_SHA               0x0007  /* 7 */
#define SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA       0x0016  /* 22 */
#define SSL_DH_anon_WITH_RC4_128_MD5            0x0018  /* 24 */
#define SSL_DH_anon_WITH_3DES_EDE_CBC_SHA       0x001B  /* 27 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA        0x0033  /* 51 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA        0x0039  /* 57 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256     0x0067  /* 103 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256     0x006B  /* 107 */
#define TLS_DH_anon_WITH_AES_128_CBC_SHA        0x0034  /* 52 */
#define TLS_DH_anon_WITH_AES_256_CBC_SHA        0x003A  /* 58 */
#define TLS_RSA_WITH_AES_128_CBC_SHA256         0x003C  /* 60 */
#define TLS_RSA_WITH_AES_256_CBC_SHA256         0x003D  /* 61 */
#define TLS_RSA_WITH_SEED_CBC_SHA               0x0096  /* 150 */
#define TLS_PSK_WITH_AES_128_CBC_SHA            0x008C  /* 140 */
#define TLS_PSK_WITH_AES_128_CBC_SHA256         0x00AE  /* 174 */
#define TLS_PSK_WITH_AES_256_CBC_SHA384         0x00AF  /* 175 */
#define TLS_PSK_WITH_AES_256_CBC_SHA            0x008D  /* 141 */
#define TLS_DHE_PSK_WITH_AES_128_CBC_SHA        0x0090  /* 144 */
#define TLS_DHE_PSK_WITH_AES_256_CBC_SHA        0x0091  /* 145 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA     0xC004  /* 49156 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA     0xC005  /* 49157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    0xC009  /* 49161 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    0xC00A  /* 49162 */
#define TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA     0xC012  /* 49170 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA      0xC013  /* 49171 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA      0xC014  /* 49172 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA       0xC00E  /* 49166 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA       0xC00F  /* 49167 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 0xC023  /* 49187 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 0xC024  /* 49188 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256  0xC025  /* 49189 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384  0xC026  /* 49190 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256   0xC027  /* 49191 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384   0xC028  /* 49192 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256    0xC029  /* 49193 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384    0xC02A  /* 49194 */

#define TLS_RSA_WITH_AES_128_GCM_SHA256         0x009C  /* 156 */
#define TLS_RSA_WITH_AES_256_GCM_SHA384         0x009D  /* 157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 0xC02B  /* 49195 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 0xC02C  /* 49196 */
#define TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256  0xC02D  /* 49197 */
#define TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384  0xC02E  /* 49198 */
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   0xC02F  /* 49199 */
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   0xC030  /* 49200 */
#define TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256    0xC031  /* 49201 */
#define TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384    0xC032  /* 49202 */

//Tested against kernel.org:
//TLS 1.1
//#define TLS_MAJ 3
//#define TLS_MIN 2
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA // ok, recvs SERVER_KEY_EXCHANGE
//TLS 1.2
#define TLS_MAJ 3
#define TLS_MIN 3
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA // ok, recvs SERVER_KEY_EXCHANGE *** matrixssl uses this on my box
//#define CIPHER_ID TLS_RSA_WITH_AES_256_CBC_SHA256 // ok, no SERVER_KEY_EXCHANGE
// All GCMs:
//#define CIPHER_ID TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 // ok, recvs SERVER_KEY_EXCHANGE
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
//#define CIPHER_ID TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384
//#define CIPHER_ID TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384
//#define CIPHER_ID TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_RSA_WITH_AES_256_GCM_SHA384 // ok, no SERVER_KEY_EXCHANGE
#define CIPHER_ID TLS_RSA_WITH_AES_128_GCM_SHA256 // ok, no SERVER_KEY_EXCHANGE *** select this?
//#define CIPHER_ID TLS_DH_anon_WITH_AES_256_CBC_SHA // SSL_ALERT_HANDSHAKE_FAILURE
//^^^^^^^^^^^^^^^^^^^^^^^ (tested b/c this one doesn't req server certs... no luck)
//test TLS_RSA_WITH_AES_128_CBC_SHA, in tls 1.2 it's mandated to be always supported

struct record_hdr {
	uint8_t type;
	uint8_t proto_maj, proto_min;
	uint8_t len16_hi, len16_lo;
};

typedef struct tls_state {
	int fd;

	psRsaKey_t server_rsa_pub_key;

	// RFC 5246
	// |6.2.1. Fragmentation
	// |  The record layer fragments information blocks into TLSPlaintext
	// |  records carrying data in chunks of 2^14 bytes or less.  Client
	// |  message boundaries are not preserved in the record layer (i.e.,
	// |  multiple client messages of the same ContentType MAY be coalesced
	// |  into a single TLSPlaintext record, or a single message MAY be
	// |  fragmented across several records)
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSPlaintext.fragment.
	// |    The length MUST NOT exceed 2^14.
	// |...
	// | 6.2.2. Record Compression and Decompression
	// |...
	// |  Compression must be lossless and may not increase the content length
	// |  by more than 1024 bytes.  If the decompression function encounters a
	// |  TLSCompressed.fragment that would decompress to a length in excess of
	// |  2^14 bytes, it MUST report a fatal decompression failure error.
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSCompressed.fragment.
	// |    The length MUST NOT exceed 2^14 + 1024.
	//
	// Since our buffer also contains 5-byte headers, make it a bit bigger:
	int insize;
	int tail;
	uint8_t inbuf[18*1024];
} tls_state_t;

void tls_get_random(void *buf, unsigned len)
{
	if (len != open_read_close("/dev/urandom", buf, len))
		xfunc_die();
}

static
tls_state_t *new_tls_state(void)
{
	tls_state_t *tls = xzalloc(sizeof(*tls));
	tls->fd = -1;
	return tls;
}

static unsigned get24be(const uint8_t *p)
{
	return 0x100*(0x100*p[0] + p[1]) + p[2];
}

static void dump(const void *vp, int len)
{
	char hexbuf[32 * 1024 + 4];
	const uint8_t *p = vp;

	while (len > 0) {
		unsigned xhdr_len;
		if (len < 5) {
			bin2hex(hexbuf, (void*)p, len)[0] = '\0';
			dbg("< |%s|\n", hexbuf);
			return;
		}
		xhdr_len = 0x100*p[3] + p[4];
		dbg("< hdr_type:%u ver:%u.%u len:%u", p[0], p[1], p[2], xhdr_len);
		p += 5;
		len -= 5;
		if (len >= 4 && p[-5] == RECORD_TYPE_HANDSHAKE) {
			unsigned len24 = get24be(p + 1);
			dbg(" type:%u len24:%u", p[0], len24);
		}
		if (xhdr_len > len)
			xhdr_len = len;
		bin2hex(hexbuf, (void*)p, xhdr_len)[0] = '\0';
		dbg(" |%s|\n", hexbuf);
		p += xhdr_len;
		len -= xhdr_len;
	}
}

static void tls_error_die(tls_state_t *tls)
{
	dump(tls->inbuf, tls->insize + tls->tail);
	xfunc_die();
}

static int xread_tls_block(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	int len;
	int total;
	int target;

	dbg("insize:%u tail:%u\n", tls->insize, tls->tail);
	memmove(tls->inbuf, tls->inbuf + tls->insize, tls->tail);
	errno = 0;
	total = tls->tail;
	target = sizeof(tls->inbuf);
	for (;;) {
		if (total >= sizeof(*xhdr) && target == sizeof(tls->inbuf)) {
			xhdr = (void*)tls->inbuf;
			target = sizeof(*xhdr) + (0x100 * xhdr->len16_hi + xhdr->len16_lo);
			if (target >= sizeof(tls->inbuf)) {
				/* malformed input (too long): yell and die */
				tls->tail = 0;
				tls->insize = total;
				tls_error_die(tls);
			}
			// can also check type/proto_maj/proto_min here
		}
		/* if total >= target, we have a full packet (and possibly more)... */
		if (total - target >= 0)
			break;
		len = safe_read(tls->fd, tls->inbuf + total, sizeof(tls->inbuf) - total);
		if (len <= 0)
			bb_perror_msg_and_die("short read");
		total += len;
	}
	tls->tail = total - target;
	tls->insize = target;
	target -= sizeof(*xhdr);
	dbg("got block len:%u\n", target);
	return target;
}

static int xread_tls_handshake_block(tls_state_t *tls, int min_len)
{
	struct record_hdr *xhdr;
	int len = xread_tls_block(tls);

	xhdr = (void*)tls->inbuf;
	if (len < min_len
	 || xhdr->type != RECORD_TYPE_HANDSHAKE
	 || xhdr->proto_maj != TLS_MAJ
	 || xhdr->proto_min != TLS_MIN
	) {
		tls_error_die(tls);
	}
	dbg("got HANDSHAKE\n");
	return len;
}

static unsigned get_der_len(uint8_t **bodyp, uint8_t *der, uint8_t *end)
{
	unsigned len, len1;

	if (end - der < 2)
		xfunc_die();
//	if ((der[0] & 0x1f) == 0x1f) /* not single-byte item code? */
//		xfunc_die();

	len = der[1]; /* maybe it's short len */
	if (len >= 0x80) {
		/* no */
		if (end - der < (int)(len - 0x7e)) /* need 3 or 4 bytes for 81, 82 */
			xfunc_die();

		len1 = der[2];
		if (len == 0x81) {
			/* it's "ii 81 xx" */
		} else if (len == 0x82) {
			/* it's "ii 82 xx yy" */
			len1 = 0x100*len1 + der[3];
			der += 1; /* skip [yy] */
		} else {
			/* 0x80 is "0 bytes of len", invalid DER: must use short len if can */
			/* >0x82 is "3+ bytes of len", should not happen realistically */
			xfunc_die();
		}
		der += 1; /* skip [xx] */
		len = len1;
//		if (len < 0x80)
//			xfunc_die(); /* invalid DER: must use short len if can */
	}
	der += 2; /* skip [code]+[1byte] */

	if (end - der < (int)len)
		xfunc_die();
	*bodyp = der;

	return len;
}

static uint8_t *enter_der_item(uint8_t *der, uint8_t **endp)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, *endp);
	dbg("entered der @%p:0x%02x len:%u inner_byte @%p:0x%02x\n", der, der[0], len, new_der, new_der[0]);
	/* Move "end" position to cover only this item */
	*endp = new_der + len;
	return new_der;
}

static uint8_t *skip_der_item(uint8_t *der, uint8_t *end)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, end);
	/* Skip body */
	new_der += len;
	dbg("skipped der 0x%02x, next byte 0x%02x\n", der[0], new_der[0]);
	return new_der;
}

static void der_binary_to_pstm(pstm_int *pstm_n, uint8_t *der, uint8_t *end)
{
	uint8_t *bin_ptr;
	unsigned len = get_der_len(&bin_ptr, der, end);

	dbg("binary bytes:%u, first:0x%02x\n", len, bin_ptr[0]);
	pstm_init_for_read_unsigned_bin(/*pool:*/ NULL, pstm_n, len);
	pstm_read_unsigned_bin(pstm_n, bin_ptr, len);
	//return bin + len;
}

static void find_key_in_der_cert(tls_state_t *tls, uint8_t *der, int len)
{
/* Certificate is a DER-encoded data structure. Each DER element has a length,
 * which makes it easy to skip over large compound elements of any complexity
 * without parsing them. Example: partial decode of kernel.org certificate:
 *  SEQ 0x05ac/1452 bytes (Certificate): 308205ac
 *    SEQ 0x0494/1172 bytes (tbsCertificate): 30820494
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0] 3 bytes: a003
 *        INTEGER (version): 0201 02
 *      INTEGER 0x11 bytes (serialNumber): 0211 00 9f85bf664b0cddafca508679501b2be4
 *      //^^^^^^note: matrixSSL also allows [ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 2] = 0x82 type
 *      SEQ 0x0d bytes (signatureAlgo): 300d
 *        OID 9 bytes: 0609 2a864886f70d01010b (OID_SHA256_RSA_SIG 42.134.72.134.247.13.1.1.11)
 *        NULL: 0500
 *      SEQ 0x5f bytes (issuer): 305f
 *        SET 11 bytes: 310b
 *          SEQ 9 bytes: 3009
 *            OID 3 bytes: 0603 550406
 *            Printable string "FR": 1302 4652
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550408
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550407
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 55040a
 *            Printable string "Gandi": 1305 47616e6469
 *        SET 32 bytes: 3120
 *          SEQ 30 bytes: 301e
 *            OID 3 bytes: 0603 550403
 *            Printable string "Gandi Standard SSL CA 2": 1317 47616e6469205374616e646172642053534c2043412032
 *      SEQ 30 bytes (validity): 301e
 *        TIME "161011000000Z": 170d 3136313031313030303030305a
 *        TIME "191011235959Z": 170d 3139313031313233353935395a
 *      SEQ 0x5b/91 bytes (subject): 305b //I did not decode this
 *          3121301f060355040b1318446f6d61696e20436f
 *          6e74726f6c2056616c6964617465643121301f06
 *          0355040b1318506f73697469766553534c204d75
 *          6c74692d446f6d61696e31133011060355040313
 *          0a6b65726e656c2e6f7267
 *      SEQ 0x01a2/418 bytes (subjectPublicKeyInfo): 308201a2
 *        SEQ 13 bytes (algorithm): 300d
 *          OID 9 bytes: 0609 2a864886f70d010101 (OID_RSA_KEY_ALG 42.134.72.134.247.13.1.1.1)
 *          NULL: 0500
 *        BITSTRING 0x018f/399 bytes (publicKey): 0382018f
 *          ????: 00
 *          //after the zero byte, it appears key itself uses DER encoding:
 *          SEQ 0x018a/394 bytes: 3082018a
 *            INTEGER 0x0181/385 bytes (modulus): 02820181
 *                  00b1ab2fc727a3bef76780c9349bf3
 *                  ...24 more blocks of 15 bytes each...
 *                  90e895291c6bc8693b65
 *            INTEGER 3 bytes (exponent): 0203 010001
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0x3] 0x01e5 bytes (X509v3 extensions): a38201e5
 *        SEQ 0x01e1 bytes: 308201e1
 *        ...
 * Certificate is a sequence of three elements:
 *	tbsCertificate (SEQ)
 *	signatureAlgorithm (AlgorithmIdentifier)
 *	signatureValue (BIT STRING)
 *
 * In turn, tbsCertificate is a sequence of:
 *	version
 *	serialNumber
 *	signatureAlgo (AlgorithmIdentifier)
 *	issuer (Name, has complex structure)
 *	validity (Validity, SEQ of two Times)
 *	subject (Name)
 *	subjectPublicKeyInfo (SEQ)
 *	...
 *
 * subjectPublicKeyInfo is a sequence of:
 *	algorithm (AlgorithmIdentifier)
 *	publicKey (BIT STRING)
 *
 * We need Certificate.tbsCertificate.subjectPublicKeyInfo.publicKey
 */
	uint8_t *end = der + len;

	/* enter "Certificate" item: [der, end) will be only Cert */
	der = enter_der_item(der, &end);

	/* enter "tbsCertificate" item: [der, end) will be only tbsCert */
	der = enter_der_item(der, &end);

	/* skip up to subjectPublicKeyInfo */
	der = skip_der_item(der, end); /* version */
	der = skip_der_item(der, end); /* serialNumber */
	der = skip_der_item(der, end); /* signatureAlgo */
	der = skip_der_item(der, end); /* issuer */
	der = skip_der_item(der, end); /* validity */
	der = skip_der_item(der, end); /* subject */

	/* enter subjectPublicKeyInfo */
	der = enter_der_item(der, &end);
	{ /* check subjectPublicKeyInfo.algorithm */
		static const uint8_t expected[] = {
			0x30,0x0d, // SEQ 13 bytes
			0x06,0x09, 0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01, // OID RSA_KEY_ALG 42.134.72.134.247.13.1.1.1
			//0x05,0x00, // NULL
		};
		if (memcmp(der, expected, sizeof(expected)) != 0)
			bb_error_msg_and_die("not RSA key");
	}
	/* skip subjectPublicKeyInfo.algorithm */
	der = skip_der_item(der, end);
	/* enter subjectPublicKeyInfo.publicKey */
//	die_if_not_this_der_type(der, end, 0x03); /* must be BITSTRING */
	der = enter_der_item(der, &end);

	/* parse RSA key: */
//based on getAsnRsaPubKey(), pkcs1ParsePrivBin() is also of note
	dbg("key bytes:%u, first:0x%02x\n", (int)(end - der), der[0]);
	if (end - der < 14) xfunc_die();
	/* example format:
	 * ignore bits: 00
	 * SEQ 0x018a/394 bytes: 3082018a
	 *   INTEGER 0x0181/385 bytes (modulus): 02820181 XX...XXX
	 *   INTEGER 3 bytes (exponent): 0203 010001
	 */
	if (*der != 0) /* "ignore bits", should be 0 */
		xfunc_die();
	der++;
	der = enter_der_item(der, &end); /* enter SEQ */
	//memset(tls->server_rsa_pub_key, 0, sizeof(tls->server_rsa_pub_key));
	der_binary_to_pstm(&tls->server_rsa_pub_key.N, der, end); /* modulus */
	der = skip_der_item(der, end);
	der_binary_to_pstm(&tls->server_rsa_pub_key.e, der, end); /* exponent */
	tls->server_rsa_pub_key.size = pstm_unsigned_bin_size(&tls->server_rsa_pub_key.N);
	dbg("server_rsa_pub_key.size:%d\n", tls->server_rsa_pub_key.size);
}

/*
 * TLS Handshake routines
 */
static void send_client_hello(tls_state_t *tls)
{
	struct client_hello {
		struct record_hdr xhdr;
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32];
		uint8_t session_id_len;
		/* uint8_t session_id[]; */
		uint8_t cipherid_len16_hi, cipherid_len16_lo;
		uint8_t cipherid[2 * 1]; /* actually variable */
		uint8_t comprtypes_len;
		uint8_t comprtypes[1]; /* actually variable */
	};
	struct client_hello hello;

	memset(&hello, 0, sizeof(hello));
	hello.xhdr.type = RECORD_TYPE_HANDSHAKE;
	hello.xhdr.proto_maj = TLS_MAJ;
	hello.xhdr.proto_min = TLS_MIN;
	//zero: hello.xhdr.len16_hi = (sizeof(hello) - sizeof(hello.xhdr)) >> 8;
	hello.xhdr.len16_lo = (sizeof(hello) - sizeof(hello.xhdr));
	hello.type = HANDSHAKE_CLIENT_HELLO;
	//hello.len24_hi  = 0;
	//zero: hello.len24_mid = (sizeof(hello) - sizeof(hello.xhdr) - 4) >> 8;
	hello.len24_lo  = (sizeof(hello) - sizeof(hello.xhdr) - 4);
	hello.proto_maj = TLS_MAJ;	/* the "requested" version of the protocol, */
	hello.proto_min = TLS_MIN;	/* can be higher than one in record headers */
	tls_get_random(hello.rand32, sizeof(hello.rand32));
	//hello.session_id_len = 0;
	//hello.cipherid_len16_hi = 0;
	hello.cipherid_len16_lo = 2 * 1;
	hello.cipherid[0] = CIPHER_ID >> 8;
	hello.cipherid[1] = CIPHER_ID & 0xff;
	hello.comprtypes_len = 1;
	//hello.comprtypes[0] = 0;

	xwrite(tls->fd, &hello, sizeof(hello));
#if 0 /* dump */
	for (;;) {
		uint8_t buf[16*1024];
		sleep(2);
		len = recv(tls->fd, buf, sizeof(buf), 0); //MSG_DONTWAIT);
		if (len < 0) {
			if (errno == EAGAIN)
				continue;
			bb_perror_msg_and_die("recv");
		}
		if (len == 0)
			break;
		dump(buf, len);
	}
#endif
}

static void get_server_hello(tls_state_t *tls)
{
	struct server_hello {
		struct record_hdr xhdr;
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32]; /* first 4 bytes are unix time in BE format */
		uint8_t session_id_len;
		uint8_t session_id[32];
		uint8_t cipherid_hi, cipherid_lo;
		uint8_t comprtype;
		/* extensions may follow, but only those which client offered in its Hello */
	};
	struct server_hello *hp;

	xread_tls_handshake_block(tls, 74);

	hp = (void*)tls->inbuf;
	// 74 bytes:
	// 02  000046 03|03   58|78|cf|c1 50|a5|49|ee|7e|29|48|71|fe|97|fa|e8|2d|19|87|72|90|84|9d|37|a3|f0|cb|6f|5f|e3|3c|2f |20  |d8|1a|78|96|52|d6|91|01|24|b3|d6|5b|b7|d0|6c|b3|e1|78|4e|3c|95|de|74|a0|ba|eb|a7|3a|ff|bd|a2|bf |00|9c |00|
	//SvHl len=70 maj.min unixtime^^^ 28randbytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^ slen sid32bytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cipSel comprSel
	if (hp->type != HANDSHAKE_SERVER_HELLO
	 || hp->len24_hi  != 0
	 || hp->len24_mid != 0
	 || hp->len24_lo  != 70
	 || hp->proto_maj != TLS_MAJ
	 || hp->proto_min != TLS_MIN
	 || hp->session_id_len != 32
	 || hp->cipherid_hi != (CIPHER_ID >> 8)
	 || hp->cipherid_lo != (CIPHER_ID & 0xff)
	 || hp->comprtype != 0
	) {
		tls_error_die(tls);
	}
	dbg("got SERVER_HELLO\n");
}

static void get_server_cert(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	uint8_t *certbuf;
	int len, len1;

	len = xread_tls_handshake_block(tls, 10);

	xhdr = (void*)tls->inbuf;
	certbuf = (void*)(xhdr + 1);
	if (certbuf[0] != HANDSHAKE_CERTIFICATE)
		tls_error_die(tls);
	dbg("got CERTIFICATE\n");
	// 4392 bytes:
	// 0b  00|11|24 00|11|21 00|05|b0 30|82|05|ac|30|82|04|94|a0|03|02|01|02|02|11|00|9f|85|bf|66|4b|0c|dd|af|ca|50|86|79|50|1b|2b|e4|30|0d...
	//Cert len=4388 ChainLen CertLen^ DER encoded X509 starts here. openssl x509 -in FILE -inform DER -noout -text
	len1 = get24be(certbuf + 1);
	if (len1 > len - 4) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 4);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 7);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;

	if (len)
		find_key_in_der_cert(tls, certbuf + 10, len);
}

static void send_client_key_exchange(tls_state_t *tls)
{
	struct client_key_exchange {
		struct record_hdr xhdr;
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t keylen16_hi, keylen16_lo; /* exist for RSA, but not for some other key types */
//had a bug when had no keylen: we:
//write(3, "\x16\x03\x03\x01\x84\x10\x00\x01\x80\xXX\xXX\xXX\xXX\xXX\xXX...", 393) = 393
//openssl:
//write to 0xe9a090 [0xf9ac20] (395 bytes => 395 (0x18B))
//0000 -      16  03  03  01  86  10  00  01 -82  01  80  xx  xx  xx  xx  xx
		uint8_t key[384]; // size??
	};
	struct client_key_exchange record;
	uint8_t rsa_premaster[SSL_HS_RSA_PREMASTER_SIZE];

	memset(&record, 0, sizeof(record));
	record.xhdr.type = RECORD_TYPE_HANDSHAKE;
	record.xhdr.proto_maj = TLS_MAJ;
	record.xhdr.proto_min = TLS_MIN;
	record.xhdr.len16_hi = (sizeof(record) - sizeof(record.xhdr)) >> 8;
	record.xhdr.len16_lo = (sizeof(record) - sizeof(record.xhdr)) & 0xff;
	record.type = HANDSHAKE_CLIENT_KEY_EXCHANGE;
	//record.len24_hi  = 0;
	record.len24_mid = (sizeof(record) - sizeof(record.xhdr) - 4) >> 8;
	record.len24_lo  = (sizeof(record) - sizeof(record.xhdr) - 4) & 0xff;
	record.keylen16_hi = (sizeof(record) - sizeof(record.xhdr) - 6) >> 8;
	record.keylen16_lo = (sizeof(record) - sizeof(record.xhdr) - 6) & 0xff;

	tls_get_random(rsa_premaster, sizeof(rsa_premaster));
// RFC 5246
// "Note: The version number in the PreMasterSecret is the version
// offered by the client in the ClientHello.client_version, not the
// version negotiated for the connection."
	rsa_premaster[0] = TLS_MAJ;
	rsa_premaster[1] = TLS_MIN;
	psRsaEncryptPub(/*pool:*/ NULL,
		/* psRsaKey_t* */ &tls->server_rsa_pub_key,
		rsa_premaster, /*inlen:*/ sizeof(rsa_premaster),
		record.key, sizeof(record.key),
		data_param_ignored
	);

	xwrite(tls->fd, &record, sizeof(record));
}

static void send_change_cipher_spec(tls_state_t *tls)
{
	static const uint8_t rec[] = {
		RECORD_TYPE_CHANGE_CIPHER_SPEC, TLS_MAJ, TLS_MIN, 00, 01,
		01
	};
	xwrite(tls->fd, rec, sizeof(rec));
}

static void send_client_finished(tls_state_t *tls)
{
// RFC 5246 on pseudorandom function (PRF):
//
// 5.  HMAC and the Pseudorandom Function
//...
// In this section, we define one PRF, based on HMAC.  This PRF with the
// SHA-256 hash function is used for all cipher suites defined in this
// document and in TLS documents published prior to this document when
// TLS 1.2 is negotiated.
//...
//    P_hash(secret, seed) = HMAC_hash(secret, A(1) + seed) +
//                           HMAC_hash(secret, A(2) + seed) +
//                           HMAC_hash(secret, A(3) + seed) + ...
// where + indicates concatenation.
// A() is defined as:
//    A(0) = seed
//    A(i) = HMAC_hash(secret, A(i-1))
// P_hash can be iterated as many times as necessary to produce the
// required quantity of data.  For example, if P_SHA256 is being used to
// create 80 bytes of data, it will have to be iterated three times
// (through A(3)), creating 96 bytes of output data; the last 16 bytes
// of the final iteration will then be discarded, leaving 80 bytes of
// output data.
//
// TLS's PRF is created by applying P_hash to the secret as:
//
//    PRF(secret, label, seed) = P_<hash>(secret, label + seed)
//
// The label is an ASCII string.

	tls->fd = 0;

// 7.4.9.  Finished
// A Finished message is always sent immediately after a change
// cipher spec message to verify that the key exchange and
// authentication processes were successful.  It is essential that a
// change cipher spec message be received between the other handshake
// messages and the Finished message.
//...
// The Finished message is the first one protected with the just
// negotiated algorithms, keys, and secrets.  Recipients of Finished
// messages MUST verify that the contents are correct.  Once a side
// has sent its Finished message and received and validated the
// Finished message from its peer, it may begin to send and receive
// application data over the connection.
//...
// struct {
//     opaque verify_data[verify_data_length];
// } Finished;
//
// verify_data
//    PRF(master_secret, finished_label, Hash(handshake_messages))
//       [0..verify_data_length-1];
//
// finished_label
//    For Finished messages sent by the client, the string
//    "client finished".  For Finished messages sent by the server,
//    the string "server finished".
//
// Hash denotes a Hash of the handshake messages.  For the PRF
// defined in Section 5, the Hash MUST be the Hash used as the basis
// for the PRF.  Any cipher suite which defines a different PRF MUST
// also define the Hash to use in the Finished computation.
//
// In previous versions of TLS, the verify_data was always 12 octets
// long.  In the current version of TLS, it depends on the cipher
// suite.  Any cipher suite which does not explicitly specify
// verify_data_length has a verify_data_length equal to 12.  This
// includes all existing cipher suites.
}

static void get_change_cipher_spec(tls_state_t *tls)
{
	tls->fd = 0;
}

static void get_server_finished(tls_state_t *tls)
{
	tls->fd = 0;
}

static void tls_handshake(tls_state_t *tls)
{
	// Client              RFC 5246                Server
	// (*) - optional messages, not always sent
	//
	// ClientHello          ------->
	//                                        ServerHello
	//                                       Certificate*
	//                                 ServerKeyExchange*
	//                                CertificateRequest*
	//                      <-------      ServerHelloDone
	// Certificate*
	// ClientKeyExchange
	// CertificateVerify*
	// [ChangeCipherSpec]
	// Finished             ------->
	//                                 [ChangeCipherSpec]
	//                      <-------             Finished
	// Application Data     <------>     Application Data
	int len;

	send_client_hello(tls);
	get_server_hello(tls);

	//RFC 5246
	// The server MUST send a Certificate message whenever the agreed-
	// upon key exchange method uses certificates for authentication
	// (this includes all key exchange methods defined in this document
	// except DH_anon).  This message will always immediately follow the
	// ServerHello message.
	//
	// IOW: in practice, Certificate *always* follows.
	// (for example, kernel.org does not even accept DH_anon cipher id)
	get_server_cert(tls);

	len = xread_tls_handshake_block(tls, 4);
	if (tls->inbuf[5] == HANDSHAKE_SERVER_KEY_EXCHANGE) {
		// 459 bytes:
		// 0c   00|01|c7 03|00|17|41|04|87|94|2e|2f|68|d0|c9|f4|97|a8|2d|ef|ed|67|ea|c6|f3|b3|56|47|5d|27|b6|bd|ee|70|25|30|5e|b0|8e|f6|21|5a...
		//SvKey len=455^
		// with TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA: 461 bytes:
		// 0c   00|01|c9 03|00|17|41|04|cd|9b|b4|29|1f|f6|b0|c2|84|82|7f|29|6a|47|4e|ec|87|0b|c1|9c|69|e1|f8|c6|d0|53|e9|27|90|a5|c8|02|15|75...
		dbg("got SERVER_KEY_EXCHANGE len:%u\n", len);
//need to save it
		xread_tls_handshake_block(tls, 4);
	}
//	if (tls->inbuf[5] == HANDSHAKE_CERTIFICATE_REQUEST) {
//		dbg("got CERTIFICATE_REQUEST\n");
//RFC 5246: (in response to this,) "If no suitable certificate is available,
// the client MUST send a certificate message containing no
// certificates.  That is, the certificate_list structure has a
// length of zero. ...
// Client certificates are sent using the Certificate structure
// defined in Section 7.4.2."
// (i.e. the same format as server certs)
//		xread_tls_handshake_block(tls, 4);
//	}
	if (tls->inbuf[5] == HANDSHAKE_SERVER_HELLO_DONE) {
		// 0e 000000 (len:0)
		dbg("got SERVER_HELLO_DONE\n");
		send_client_key_exchange(tls);
		send_change_cipher_spec(tls);
//we now should be able to send encrypted... as soon as we grok AES.
		send_client_finished(tls);
		get_change_cipher_spec(tls);
		get_server_finished(tls);
//we now should receive encrypted, and application data can be sent/received
	} else {
		tls_error_die(tls);
	}
}

int tls_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tls_main(int argc UNUSED_PARAM, char **argv)
{
	tls_state_t *tls;
	len_and_sockaddr *lsa;
	int fd;

	// INIT_G();
	// getopt32(argv, "myopts")

	if (!argv[1])
		bb_show_usage();

	lsa = xhost2sockaddr(argv[1], 443);
	fd = xconnect_stream(lsa);

	tls = new_tls_state();
	tls->fd = fd;
	tls_handshake(tls);

	return EXIT_SUCCESS;
}