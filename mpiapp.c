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
 * Original creation date: 24-May-2018
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "c0appz.h"

#define DMPISEQ 0
#define DCLOVIS 1

char *prog;

/* main */
int main(int argc, char **argv)
{
	int wsize;
	int wrank;
	char pname[MPI_MAX_PROCESSOR_NAME];
	int pnlen;

	/* MPI start */
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
	MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
	MPI_Get_processor_name(pname, &pnlen);

	fprintf(stderr,"MPI rank [ %d ] start.\n",wrank);

	/*
	 * rank index
	 * gather all node ids and ranks
	 * and determine rank index.
	 */
	int node;
	node = atoi((char *)(pname+7));
	node = (node * 1000) + wrank;
	int *nptr = (int *)malloc(sizeof(int) * wsize);
	MPI_Allgather(&node, 1, MPI_INT, nptr, 1, MPI_INT, MPI_COMM_WORLD);
	MPI_Barrier(MPI_COMM_WORLD);

	int  idx;
	int *ptr;
	idx = 0;
	ptr = nptr;
	while(ptr != nptr + wsize) {
		if((*ptr/1000 == node/1000) && (node > *ptr)) idx++;
		ptr++;
	}
	fprintf(stderr,"MPI rank [ %d ] [%s] size = %d rank = %d index = %d\n",
			wrank, pname, wsize, wrank, idx);

	/* time in */
	c0appz_timein();

	prog = basename(strdup(argv[0]));

	c0appz_setrc(prog);
	c0appz_putrc();

#if DMPISEQ
	/* mpi recv */
	int token = 0;
	if(wrank != 0) {
		MPI_Recv(&token,1,MPI_INT,wrank-1,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
		fprintf(stderr,"recv:[%d] <<- [%d] token:[[%d]]\n",wrank,wrank-1,token);
	}
#endif

#if DCLOVIS
	fprintf(stderr,"MPI rank [ %d ] c0appz_init(%d) [**]\n",wrank,idx);
	/* initialize resources */
	rc = c0appz_init(idx);
	if (rc != 0) {
		fprintf(stderr,"error! c0appz_init(%i) failed: %d\n", idx, rc);
		return -2;
	}
	fprintf(stderr,"MPI rank [ %d ] c0appz_init(%d) [OK]\n",wrank,idx);

#endif

#if DMPISEQ
	/* mpi send */
	token = 1000+wrank;
	if(wrank != wsize-1) {
		MPI_Send(&token,1,MPI_INT, wrank+1,0,MPI_COMM_WORLD);
		fprintf(stderr,"send:[%d] ->> [%d] token:[[%d]]\n",wrank,wrank+1,token);
	}
	MPI_Barrier(MPI_COMM_WORLD);
#endif

	/*
	 * do something
	 */
	fprintf(stderr,"MPI rank [ %d ] do something here...\n",wrank);
#if DCLOVIS
	/* delete */
	int64_t idh = 0;
	int64_t idl = 0;
	idl = 1048577 + wrank;
	fprintf(stderr,"deleting object < %lu %lu >\n",idh,idl);
	if (c0appz_rm(0,1048577) != 0) {
		fprintf(stderr,"error! delete object failed.\n");
	};
#endif

#if DCLOVIS
	fprintf(stderr,"MPI rank [ %d ] c0appz_free() [**]\n",wrank);
	/* free resources*/
	c0appz_free();
	fprintf(stderr,"MPI rank [ %d ] c0appz_free() [OK]\n",wrank);
#endif

	/* time out */
	c0appz_timeout(0);

	/* success */
	fprintf(stderr,"MPI rank [ %d ] end.\n",wrank);

	/* MPI end */
	MPI_Finalize();

	return 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
