extern void index_sort(TBuf *buf, int pos[], int len);
extern int index_has(TBuf *idx, Tuple *t, int ipos[], int tpos[], int len);
extern TBuf *index_match(TBuf *idx, Tuple *t, int ipos[], int tpos[], int len);
