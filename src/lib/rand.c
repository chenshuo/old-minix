static long seed = 1L;

int srand(x)
unsigned x;
{
  seed = (long)x;
}

int rand()
{
  seed = (1103515245L * seed + 12345) & 0x7FFFFFFF;
  return((int) (seed & 077777));
}
