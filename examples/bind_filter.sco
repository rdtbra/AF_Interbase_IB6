#ident "InterBase V4.0 shared filter library"
#address .text 0xa4000000
#address .data 0xa4400000
#target /shlib/libgdsf_s
#branch
    FUNCTIONS_entrypoint	1
#objects
    shrfilter.o
    functions.o
    nr_filter.o
#hide linker *
#init shrfilter.o
    _libfun_strcmp		strcmp
    _libfun_sprintf		sprintf
    _libfun_system		system
    _libfun_fopen		fopen
    _libfun_unlink		unlink
    _libfun_fprintf		fprintf
    _libfun_fclose		fclose
    _libfun_fgetc		fgetc
