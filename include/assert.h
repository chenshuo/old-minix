#ifndef NDEBUG
#define assert(xxx) {if(!(xxx)){fprintf(stderr, "False assertion at line %d in file \"%s\"\n", __LINE__, __FILE__);  exit(1);}}
#endif


