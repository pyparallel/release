/* This is built as a stand-alone executable by the Makefile, and helps turn
   Lib/importlib/_bootstrap.py into a frozen module in Python/importlib.h
*/

/*
#undef WITH_PARALLEL
#define DISABLE_PARALLEL 1
*/

#include <Python.h>
#include <marshal.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MS_WINDOWS
#include <unistd.h>
#endif


/* To avoid a circular dependency on frozen.o, we create our own structure
   of frozen modules instead, left deliberately blank so as to avoid
   unintentional import of a stale version of _frozen_importlib. */

static struct _frozen _PyImport_FrozenModules[] = {
    {0, 0, 0} /* sentinel */
};

#ifndef MS_WINDOWS
/* On Windows, this links with the regular pythonXY.dll, so this variable comes
   from frozen.obj. In the Makefile, frozen.o is not linked into this executable,
   so we define the variable here. */
struct _frozen *PyImport_FrozenModules;
#endif

const char header[] = "/* Auto-generated by Modules/_freeze_importlib.c */";

int
main(int argc, char *argv[])
{
    char *inpath, *outpath;
    FILE *infile = NULL, *outfile = NULL;
    struct stat st;
    size_t text_size, data_size, n;
    char *text = NULL;
    unsigned char *data;
    PyObject *code = NULL, *marshalled = NULL;

    PyImport_FrozenModules = _PyImport_FrozenModules;

    if (argc != 3) {
        fprintf(stderr, "need to specify input and output paths\n");
        return 2;
    }
    inpath = argv[1];
    outpath = argv[2];
    infile = fopen(inpath, "rb");
    if (infile == NULL) {
        fprintf(stderr, "cannot open '%s' for reading\n", inpath);
        goto error;
    }
    if (fstat(fileno(infile), &st)) {
        fprintf(stderr, "cannot fstat '%s'\n", inpath);
        goto error;
    }
    text_size = st.st_size;
    text = (char *) malloc(text_size + 1);
    if (text == NULL) {
        fprintf(stderr, "could not allocate %ld bytes\n", (long) text_size);
        goto error;
    }
    n = fread(text, 1, text_size, infile);
    fclose(infile);
    infile = NULL;
    if (n < text_size) {
        fprintf(stderr, "read too short: got %ld instead of %ld bytes\n",
                (long) n, (long) text_size);
        goto error;
    }
    text[text_size] = '\0';

    Py_NoUserSiteDirectory++;
    Py_NoSiteFlag++;
    Py_IgnoreEnvironmentFlag++;

    Py_SetProgramName(L"./_freeze_importlib");
    /* Don't install importlib, since it could execute outdated bytecode. */
    _Py_InitializeEx_Private(1, 0);

    code = Py_CompileStringExFlags(text, "<frozen importlib._bootstrap>",
                                   Py_file_input, NULL, 0);
    if (code == NULL)
        goto error;
    free(text);
    text = NULL;

    marshalled = PyMarshal_WriteObjectToString(code, Py_MARSHAL_VERSION);
    Py_CLEAR(code);
    if (marshalled == NULL)
        goto error;

    assert(PyBytes_CheckExact(marshalled));
    data = (unsigned char *) PyBytes_AS_STRING(marshalled);
    data_size = PyBytes_GET_SIZE(marshalled);

    /* Open the file in text mode. The hg checkout should be using the eol extension,
       which in turn should cause the EOL style match the C library's text mode */
    outfile = fopen(outpath, "w");
    if (outfile == NULL) {
        fprintf(stderr, "cannot open '%s' for writing\n", outpath);
        goto error;
    }
    fprintf(outfile, "%s\n", header);
    fprintf(outfile, "unsigned char _Py_M__importlib[] = {\n");
    for (n = 0; n < data_size; n += 16) {
        size_t i, end = Py_MIN(n + 16, data_size);
        fprintf(outfile, "    ");
        for (i = n; i < end; i++) {
            fprintf(outfile, "%d,", (unsigned int) data[i]);
        }
        fprintf(outfile, "\n");
    }
    fprintf(outfile, "};\n");

    Py_CLEAR(marshalled);

    Py_Finalize();
    if (outfile) {
        if (ferror(outfile)) {
            fprintf(stderr, "error when writing to '%s'\n", outpath);
            goto error;
        }
        fclose(outfile);
    }
    return 0;

error:
    PyErr_Print();
    Py_Finalize();
    if (infile)
        fclose(infile);
    if (outfile)
        fclose(outfile);
    if (text)
        free(text);
    if (marshalled)
        Py_DECREF(marshalled);
    return 1;
}