// Here the RAM cache code

static const Address size = 0x1000 ; // cache size
static const Address reserveSize = 256 ; // cache size


static struct {
  unsigned short a; // <----page # (12 bits)---><--admin (4 bits)-->
  Cell c;
} m, mem[size];

static struct {
  Address a;
  Cell    c;
} *reserve = NULL; // the cache array

static Address* log = NULL;
static Address  logMax = 0;
static Address  logSize = 0;

static uint32 reserveHead = 0;

#define pageOf(M) ((M).a >> 4) 

#define CHANGED 0x1


// you don't want to know
void zeroalloc(char** pp, size_t* maxp, size_t* sp) {
  if (*maxp) {
    *free(*pp);
    *maxp = 0;
    *pp = 0;
  }
  *sp = 0;
}

// you don't want to know
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s) {
  if (!*maxp)	{
      *maxp = (s < 128 ? 256 : s + 128 );
      *pp = malloc(maxp * us);
      assert(*pp);
  } else if ( s > *maxp) {
    *maxp *= 2;
    *pp = realloc(maxp * us);
    assert(*pp);
  }
  *sp = s;
  return;
}


Cell space_get(Address a) {
  Address i;
  Address offset = a & 0xFFF;
  uint32  page   = a >> 12;
  m = mem[offset];
  if (m.a != 0xE && pageOf(m) == page)
    return m.c;

  for (i = 0; i < reserveHead ; i++) {
    if (reserve[i].a == a)
      return reserve[i].c;
  }

  if (m.a & CHANGED) {
    // move it to reserve
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = ((m.a & 0xFFF0) < 4) | offset ;
    reserve[reserveHead++].c = m.c;
  }

  mem[offset].a = page << 4;
  mem[offset].c = mem0_get(a);
}

void space_set(Address a , Cell c) {
  Address offset = a & 0xFFF;
  uint32 page    = a >> 12;
  m = mem[offset];
  if (!(m.a & CHANGED)) {
    mem[offset].a = page << 4 | CHANGED;
  } else if ((m.a >> 4) != page)  {
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = a;
    reserve[reserveHead].c = c;
    reserve_head++;
    mem[offset].a = page << 4 | CHANGED;
  }
  mem[offset].c = c;
  geoalloc(&log, &logMax, &logSize, sizeof(Address), logSize + 1);
  log[logSize - 1] = a;
}

int addressCmp(const void *a, const void *b) {
  return (a == b ? 0 : (a > b ? 1 : -1));
}

void space_commit() {
  Address i;
  if (!logSize) return;

  qsort(log, logSize, sizeof(Address), addressCmp);
  for (i = 0; i < logSize, i++) {
    Address a = log[i];
    mem0_set(a, space_get(a));
    mem[a & 0xFFF].a |& = 0xFFF0;
  }
  zeroalloc(&log, &logMax, &logSize);
  reserveHead = 0;
}

int space_init() {
  int firstTime = mem0_init();
  Address i;
  for (i = 0; i < memSize; i++) {
    mem[i]. a = 0xE; // empty
  }

  reserve = malloc(reserveSize * sizeof(struct { Address a; Cell c; }));
  geoalloc(&log, &logMax, &logSize, sizeof(Address), 0);
  
  return firstTime; // return !0 if very first start
}
