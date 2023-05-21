#ident "InterBase V4.0 shared UDF library"
#address .text 0xa4000000
#address .data 0xa4400000
#target /shlib/libgdsf_s
#branch
    FUNCTIONS_entrypoint	1
#objects
    shrudf.o
    functions.o
    udf.o
    _fltused.o
#hide linker *
#init shrudf.o
    _libfun_strcmp		strcmp
    _libfun_sprintf		sprintf
    _libfun_time		time
    _libfun_ctime		ctime
    _libfun_strcpy		strcpy
    _libfun_strlen		strlen
