/*
** This routine is used by the concordance function
**  It generates the values of S(t-) and G(t-), at the unique
**  event times.  No strata, no standard error, no special cases.  Just
**  be fast.
** y = matrix of survival data, 2 column, 
** wt= case weights
** sort = ordering vector: inverse death times, censors before deaths
**   (that is what is natural for concordance)
*/
#include "survS.h"

SEXP fastkm1(SEXP y2, SEXP wt2, SEXP sort2) {
    int i, k, p;
    int n, nevent;
    int dfirst, cfirst;
    double *time, *status, *wt;
    int *sort;
    double *S, *G, *nrisk;  /* survival function for S and G, number at risk */
    double stemp, gtemp, ntemp, dtemp, ctemp =0;
    double dtime, ctime;
    double *ncount, *dcount, *ccount;  /* number at risk, deaths, censors */
    static const char *outnames[]={"S", "G", "nrisk", "etime", ""};
    SEXP rlist, S2, G2, nrisk2, etime2;
    double *etime;

    n = nrows(y2);
    time = REAL(y2);
    status = time + n;
    wt  = REAL(wt2);
    sort = INTEGER(sort2);

    /*
    ** Pass 1, find the number of unique event times
    **  Data was sorted by reverse time: for a tied censor/death the
    **  censors are found first.
    ** Save the number at risk, for later use.  We want to accumulate this
    **  from oldest to newest, to avoid any roundoff error.
    ** The number of tied deaths (dcount), censors (ccount), and the time of
    **  the event (etime) are accumulated at the same time.
    */
    dtime = time[sort[0]];  /* most recently found death time */
    nevent = 0;
    ntemp = 0; dtemp=0; ctemp=0;
    ncount = (double *) ALLOC(n, sizeof(double));  /*n at risk */
    dcount = (double *) ALLOC(n, sizeof(double));  /* number of deaths */
    ccount = (double *) ALLOC(n, sizeof(double));  /* number of censors*/
    for (i=0; i<n; i++) {
	p = sort[i];
	if (dtime != time[p]) {
	    dtemp=0; 
	    ctemp=0;
	    dtime = time[p];
	    if (dcount[i-1] > 0) nevent++;  /* unique event time */
	}
	ntemp += wt[p];
	if (status[p] ==0) ctemp += wt[p]; else dtemp += wt[p];
	ncount[i] = ntemp;
	dcount[i] = dtemp;
	ccount[i] = ctemp;
    }
    if (dcount[n-1] > 0) nevent++;  
    /*
    ** create output vectors
    */
    PROTECT(rlist = mkNamed(VECSXP, outnames));
    S2 = SET_VECTOR_ELT(rlist, 0, allocVector(REALSXP, nevent));
    S  = REAL(S2);
    G2 = SET_VECTOR_ELT(rlist, 1, allocVector(REALSXP, nevent));
    G  = REAL(G2);
    nrisk2 = SET_VECTOR_ELT(rlist, 2, allocVector(REALSXP, nevent));
    nrisk  = REAL(nrisk2);
    etime2 = SET_VECTOR_ELT(rlist, 3, allocVector(REALSXP, nevent));
    etime  = REAL(etime2);

    /* 
    ** Pass 2: Create the lagged survival and censoring curves
    ** 1. Whenver we see a new event time
    **    a. write out the number at risk, current values of S and G.
    **    b. update the value of S.
    ** 2. At each new censoring time, update the value of G.  For a tied
    **    event and censoring, censors come after events.
    */
    k=0;  /* counts the output values */
    stemp =1;
    gtemp =1;
    dfirst =1;  cfirst =1;
    for (i= n-1; k< nevent; i--) { /* earliest time to last time */
	p = sort[i];
	if (status[p] ==1 && (dfirst || time[p] != dtime)) {
	    dtime = time[p];
	    dfirst = 0;
	    nrisk[k] = ncount[i];
	    S[k] = stemp;
	    G[k] = gtemp;
	    etime[k] = dtime;
	    k++;
	    /* Update S */
	    stemp = stemp * (ncount[i] - dcount[i])/ncount[i];
	}
	if (status[p] ==0 && (cfirst || time[p] != ctime)) {
	    ctime = time[p];
	    cfirst = 0;
	    gtemp = gtemp * (ncount[i] - ccount[i])/ ncount[i];
	}
    }
    
    UNPROTECT(1);
    return(rlist);
}

/*
** The case of start-stop data.  
** G (censoring distribution) will be incorrect here, so don't compute it.
** The first reason is that G is conceptually difficult for me when there is
**   delayed entry.  Should someone late get credit for avoiding censoring before
**   they enrolled, G(t), or only for censorings after, G(t)/G(entry)?
** The second is that the below was written before I refined the survflag routine
**   to deal with the not-actually-censoring at 6 and 15 of a (0,6)(6,15)(15,30)
**   subject
**  We do need number at risk and S(t-).
*/
SEXP fastkm2(SEXP y2, SEXP wt2, SEXP sort12, SEXP sort22) {
    int i, j, k, p1, p2;
    int n, nevent;
    int dfirst;
    double *tstart, *tstop, *status, *wt;
    int *sort1, *sort2;
    double *S, *nrisk;  /* survival function, number at risk */
    double stemp, ntemp, dtemp;
    double dtime;
    double *ncount, *dcount;  /* number at risk, deaths */
    static const char *outnames[]={"S", "nrisk", "etime", ""};
    SEXP rlist, S2, nrisk2, etime2;
    double *etime;

    n = nrows(y2);
    tstart = REAL(y2);
    tstop  = tstart + n;
    status = tstop + n;
    wt  = REAL(wt2);
    sort2 = INTEGER(sort22);
    sort1 = INTEGER(sort12);

    /*
    ** Pass 1, find the number at risk and number of events, at each unique
    ** event time.
    **  Data was sorted by reverse time: for a tied censor/death the
    **  censors are found first.
    */
    dtime = tstop[sort2[0]];  /* most recently found death time */
    nevent = 0;
    ntemp = 0;  
    ncount = (double *) ALLOC(n, sizeof(double));  /*n at risk */
    dcount = (double *) ALLOC(n, sizeof(double));  /* number of deaths */
    k=0;  /* tracks removals */
    for (i=0; i<n;) {
	p2 = sort2[i];
	dtime = tstop[p2];
	/* first remove any who are no longer at risk */
	for (; k<n; k++) {
	    p1 = sort1[k];
	    if (tstart[p1] >= dtime) ntemp -= wt[p1];
	    else break;
	}
	
	/* add up deaths over ties at this time */
	dtemp =0;
	for( ; i<n; i++) {
	    p2 = sort2[i];
	    if (dtime != tstop[p2]) break;

	    ntemp += wt[p2];
	    if (status[p2] ==1)  dtemp += wt[p2];
	    /* Note: only the value at the last of a set of tied times is used*/
	    ncount[i] = ntemp;
	    dcount[i] = dtemp;
	}
	if (dtemp >0) nevent++;
    }
    
    /*
    ** create output vectors
    */
    PROTECT(rlist = mkNamed(VECSXP, outnames));
    S2 = SET_VECTOR_ELT(rlist, 0, allocVector(REALSXP, nevent));
    S  = REAL(S2);
    nrisk2 = SET_VECTOR_ELT(rlist, 1, allocVector(REALSXP, nevent));
    nrisk  = REAL(nrisk2);
    etime2 = SET_VECTOR_ELT(rlist, 2, allocVector(REALSXP, nevent));
    etime  = REAL(etime2);

    /* 
    ** Pass 2: Create the lagged survival curve
    ** Whenver we see a new event time
    **    a. write out the number at risk, current values of S.
    **    b. write out the event time
    **    c. update the value of S.
    */
    k=0;  /* counts the output values */
    stemp =1;
    dfirst =1; 
    for (i= n-1; i >=0; i--) { /* earliest time to last time */
	p2 = sort2[i];
	if (status[p2] ==1 && (dfirst || tstop[p2] != dtime)) {
	    dtime = tstop[p2];
	    dfirst = 0;
	    nrisk[k] = ncount[i];
	    S[k] = stemp;
	    etime[k] = dtime;
	    k++;
	    /* Update S */
	    stemp = stemp * (ncount[i] - dcount[i])/ncount[i];
	}
    }
    
    UNPROTECT(1);
    return(rlist);
}
