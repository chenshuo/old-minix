CC=exec cc
CFLAGS=-O -I. -D_MINIX -D_POSIX_SOURCE

OBJ=add_ext.o adder.o adf4.o adf8.o cff4.o cff8.o cfi.o cfu.o cif4.o cif8.o \
    cmf4.o cmf8.o compact.o cuf4.o cuf8.o div_ext.o dvf4.o dvf8.o extend.o \
    fef4.o fef8.o fif4.o fif8.o mlf4.o mlf8.o mul_ext.o ngf4.o \
    ngf8.o nrm_ext.o sbf4.o sbf8.o sft_ext.o shifter.o sub_ext.o zrf4.o \
    zrf8.o zrf_ext.o fptrp.o


.SUFFIXES: .o .c .s
.c.o:
	$(CC) $(CFLAGS) -S $<
	@mv $*.s $*.s.tmp
	@sed -f FP.script < $*.s.tmp > $*.s;   rm $*.s.tmp
	@$(CC) $(CFLAGS) -c $*.s
	@rm $*.s
.s.o:
	$(CC) $(CFLAGS) -c $<

all: $(OBJ)

clean:	
	@mv fptrp.s fptrp.s.save
	@rm -f *.s *.o *.bak
	@mv fptrp.s.save fptrp.s
