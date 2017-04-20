#define CACHE_SIZE 100
#define BUFFER_SIZE 20

static volatile size_t readPos;
static volatile size_t writePos;
static SEM* fullSlots;
static SEM *freeSlots;
static SEM *waiter;
static size_t itemsBuffered;
static struct data cache[CACHE_SIZE];

int jbInit(void){
	waiter = semCreate(0);
	if(!waiter)
		return 1;
	fullSlots = semCreate(0);
	if(!fullSlots){
		semDestroy(waiter);
		return 1;
	}
	freeSlots = semCreate(CACHE_SIZE);
	if(!freeSlots){
		semDestroy(fullSlots);
		semDestroy(waiter);
		return 1;
	}
	itemsBuffered = 0;
	readPos = 0;
	writePos = 0;
	return 0;
}

void jbPut(const struct data *data){
	P(freeSlots);
	if(itmesBuffered < BUFFER_SIZE)
		itemsBuffered++;
	if(itemsBuffered == BUFFER_SIZE)
		V(waiter);
	cache[writePos] = *data;
	writePos = (writePos + 1)%CACHE_SIZE;
	V(fullSlots);
}

void jbGet(struct data *data){
	P(fullSlots);
	P(waiter);
	V(waiter);
	*data = cache[readPos];
	readPos = (readPos + 1)%CACHE_SIZE;
	V(freeSlots);
}

void jbFlush(void){
	V(waiter);
}
