/* -*- C -*- */
/*
 * COPYRIGHT 2014 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
 * Original creation date: 24-Jan-2017
 *
 * compile:
 * gcc -Wall -lssl -lcrypto c0fidgen.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <inttypes.h>
#include <time.h>
#include <openssl/md5.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

#ifndef DEBUG
#define DEBUG 0
#endif

static int c0fgen_counter = 0;

/*
 *******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 *******************************************************************************
 */
static int m_addr(char *mbuf, int msz);

/*
 *******************************************************************************
 * EXTERN FUNCTIONS
 *******************************************************************************
 */

/*
 * c0appz_generate_id()
 * generate Motr fids
 */
int c0appz_generate_id(int64_t *idh, int64_t *idl)
{
    MD5_CTX c;
    unsigned char chksum[16];
    char buf[512];

	*idh = 0;
	*idl = 0;

	/* init */
    MD5_Init(&c);

    /* utc */
    memset(buf, 0x00, 512);
    sprintf(buf, "%d", (int)time(NULL));
	#if DEBUG
    fprintf(stderr, "%s\n", buf);
    fprintf(stderr, "sz = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /* srandom */
	srandom(0);
    memset(buf, 0x00, 512);
    sprintf(buf, "%d", (int)random());
	#if DEBUG
    fprintf(stderr, "%s\n", buf);
    fprintf(stderr, "sz = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /*
     * TO DO
     * add more salt here.
     */

    /* local MAC/IP addresses */
	if(m_addr(buf,512)!=0){
		fprintf(stderr,"error! insufficient salt.\n");
		fprintf(stderr,"ioctl() could not obtain mac addresses.\n");
		return -2;
	}
	#if DEBUG
	fprintf(stderr, "%s:\n",__FUNCTION__);
	fprintf(stderr,"%s",buf);
	fprintf(stderr,"size = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /* counter */
    sprintf(buf, "%d", c0fgen_counter);
    MD5_Update(&c, buf, strlen(buf));
    c0fgen_counter++;
	#if DEBUG
    fprintf(stderr, "[ counter = %s ]\n", buf);
	#endif

	/* final */
    MD5_Final(chksum, &c);

	#if DEBUG
    int n;
    fprintf(stderr, "md5: ");
    for(n=0; n<MD5_DIGEST_LENGTH; n++)
    	fprintf(stderr, "%02x", chksum[n]);
    fprintf(stderr, "\n");
	#endif

	#if DEBUG
	fprintf(stderr, "[%" PRId64 ", " "%" PRId64 "]", *idh, *idl);
	fprintf(stderr, "\n");
	#endif

	memmove(idh, &chksum[0], sizeof(int64_t));
	memmove(idl, &chksum[8], sizeof(int64_t));

	#if DEBUG
	printf("%" PRId64 " " "%" PRId64, *idh,*idl);
	printf("\n");
	#endif

	/* success */
	return 0;
}

/*
 *******************************************************************************
 * STATIC FUNCTIONS
 *******************************************************************************
 */

/*
 * m_addr()
 */
static int m_addr(char *mbuf, int msz)
{
	char buf[8192] = {0};
	struct ifconf ifc = {0};
	struct ifreq *ifr = NULL;
	int sock = 0;						/* socket				*/
	int nifs = 0;						/* number of interfaces	*/
	char macp[19];						/* mac address buffer	*/
	char ip[INET6_ADDRSTRLEN] = {0};	/* ip address			*/
	int i = 0;
	struct ifreq *item;
	struct sockaddr *addr;

	/* clear buffer */
	memset(mbuf, 0x00, msz);

	/* get a socket handle. */
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
	{
		perror("socket");
		return 1;
	}

	/* query available interfaces. */
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(sock, SIOCGIFCONF, &ifc) < 0)
	{
		perror("ioctl(SIOCGIFCONF)");
		return 2;
	}

	/* iterate through the list of interfaces. */
	ifr = ifc.ifc_req;
	nifs = ifc.ifc_len / sizeof(struct ifreq);

	for(i = 0; i < nifs; i++)
	{
		item = &ifr[i];
		addr = &(item->ifr_addr);

		/* get the IP address*/
		if(ioctl(sock, SIOCGIFADDR, item) < 0)
		{
			perror("ioctl(OSIOCGIFADDR)");
		}

		if(!inet_ntop(AF_INET,
				&(((struct sockaddr_in *)addr)->sin_addr),ip,sizeof(ip)))
		{
			perror("inet_ntop");
			continue;
		}

		/* get the MAC address */
		if(ioctl(sock, SIOCGIFHWADDR, item) < 0)
		{
			perror("ioctl(SIOCGIFHWADDR)");
			return 3;
		}

		/* display result */
		sprintf(macp, "%02x:%02x:%02x:%02x:%02x:%02x",
				(unsigned char)item->ifr_hwaddr.sa_data[0],
				(unsigned char)item->ifr_hwaddr.sa_data[1],
				(unsigned char)item->ifr_hwaddr.sa_data[2],
				(unsigned char)item->ifr_hwaddr.sa_data[3],
				(unsigned char)item->ifr_hwaddr.sa_data[4],
				(unsigned char)item->ifr_hwaddr.sa_data[5]
				);

		#if DEBUG
		fprintf(stderr, "%s: ",__FUNCTION__);
		fprintf(stderr,"mac -> [%s] ip -> [%16s]\n", macp, ip);
		#endif
		sprintf(mbuf+strlen(mbuf),"mac -> [%s] ip -> [%16s]\n", macp, ip);

	}

	/* success */
	return 0;
}

/*
 *******************************************************************************
 * END
 *******************************************************************************
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
