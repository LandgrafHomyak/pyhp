#include <string.h>

#include <Python.h>
#include <structmember.h>
#include "pyhp.h"

#include "parser.h"
#include "encoding.h"

static const char open_block_3_tag[] = "<?python3";
static const char open_inline_3_tag[] = "<?3=";
static const char open_block_2_tag[] = "<?python2";
static const char open_inline_2_tag[] = "<?2=";
static const char close_tag[] = "?>";


PyHP_LOW_API void PyHP_Core_Parser_Init(const char *string, Py_ssize_t len, PyHP_Core_ParserIteratorState *state)
{
    state->pos = 0;
    state->index = 0;
}

PyHP_LOW_API PyHP_Core_ParserMatch PyHP_Core_Parser_Next(const char *string, Py_ssize_t len, PyHP_Core_ParserIteratorState *state)
{
    Py_ssize_t i;
    PyHP_Core_ParserMatch match;
    Py_ssize_t code_end;
    Py_ssize_t newline_pos;

    switch (state->index)
    {
        case 1:
            match.type = PyHP_Core_StatementType_RAW;
            match.start = state->raw_start;
            match.end = state->raw_end;
            if (state->code_match.type == PyHP_Core_StatementType_NONE)
            {
                state->index = -1;
            }
            else
            {
                state->index = 2;
            }
            return match;
        case 2:
            state->index = 0;
            return state->code_match;
        case -1:
            match.type = PyHP_Core_StatementType_NONE;
            return match;
        case 0:
            break;
        default:
            match.type = PyHP_Core_StatementType_NONE;
            return match;
    }

    state->code_match.type = PyHP_Core_StatementType_NONE;
    newline_pos = state->pos - 1;
    for (i = state->pos; i < len; i++)
    {
        switch (string[i])
        {
            case '\n':
                newline_pos = i;
                break;
            case '?':
                if (len - i >= 2 && string[i + 1] == '>')
                {
                    code_end = i;
                    for (i = state->pos; i < code_end; i++)
                    {
                        if (string[i] == '<' /* && len - i >= 2 */ && string[i + 1] == '?') /* check not require because i < code_end <= len - 2 */
                        {
                            if (i > newline_pos && code_end - i >= 4 &&
                                (string[i + 2] == '3' || string[i + 2] == '2') &&
                                string[i + 3] == '='
                                    )
                            {
                                switch (string[i + 2])
                                {
                                    case '1':
                                        state->code_match.type = PyHP_Core_StatementType_INLINE1;
                                        break;
                                    case '2':
                                        state->code_match.type = PyHP_Core_StatementType_INLINE2;
                                        break;
                                    case '3':
                                        state->code_match.type = PyHP_Core_StatementType_INLINE3;
                                        break;
                                }
                                state->raw_start = state->pos;
                                state->code_match.start = i + 4;
                                goto found;
                            }
                            if (code_end - i >= 10 &&
                                string[i + 2] == 'p' &&
                                string[i + 3] == 'y' &&
                                string[i + 4] == 't' &&
                                string[i + 5] == 'h' &&
                                string[i + 6] == 'o' &&
                                string[i + 7] == 'n' &&
                                (string[i + 8] == '2' || string[i + 8] == '3') &&
                                (string[i + 9] == ' ' || string[i + 9] == '\n' || string[i + 9] == '\t')
                                    )
                            {
                                switch (string[i + 8])
                                {
                                    case '1':
                                        state->code_match.type = PyHP_Core_StatementType_BLOCK1;
                                        break;
                                    case '2':
                                        state->code_match.type = PyHP_Core_StatementType_BLOCK2;
                                        break;
                                    case '3':
                                        state->code_match.type = PyHP_Core_StatementType_BLOCK3;
                                        break;
                                }
                                state->raw_start = state->pos;
                                state->code_match.start = i + 9;
                                goto found;
                            }
                        }
                    }
                }
                break;
        }
    }

    if (state->pos == len)
    {
        state->index = -1;
    }
    else
    {
        state->index = 1;
        state->raw_start = state->pos;
        state->raw_end = len;
        state->code_match.type = PyHP_Core_StatementType_NONE;
    }
    return PyHP_Core_Parser_Next(string, len, state);

    found:
    state->pos = code_end + 2;
    state->raw_end = i;
    state->code_match.end = code_end;

    if (state->raw_start == state->raw_end)
    {
        state->index = 2;
    }
    else
    {
        state->index = 1;
    }

    return PyHP_Core_Parser_Next(string, len, state);
}

PyHP_LOW_API Py_ssize_t PyHP_Core_AlignCode(char *dst, const char *src, const Py_ssize_t start, const Py_ssize_t len)
{
    Py_ssize_t d_pos; /* position on destination string */
    Py_ssize_t pos; /* position on source string */
    Py_ssize_t i_pos; /* offset position in previous indent */
    Py_ssize_t i_start; /* start position of previous offset (including common part) */
    Py_ssize_t i_common; /* len of common indent */
    Py_ssize_t i_len; /* len of previous indent (including common part) */
    Py_ssize_t c_start; /* start position of current line */
    Py_ssize_t c_line; /* number of current line, for errors formatting */
    src += start;

    /* remove indent from first line (line after <? literal) */
    d_pos = 0;
    for (pos = 0; pos < len && src[pos] != '\n'; pos++)
    {
        if (d_pos != 0 || (src[pos] != ' ' && src[pos] != '\t'))
        {
            dst[d_pos++] = src[pos];
            /* printf("!* %d '%c'\n", pos, src[pos]); */
        }
        /* else
        {
            printf("!_ %d\n", pos);
        } */
    }
    dst[d_pos] = '\n';
    pos++;

    /* calc common indent that based on second line */
    i_start = pos;
    for (; pos < len && src[pos] != '\n'; pos++)
    {
        if (src[pos] != ' ' && src[pos] != '\t')
        {
            break;
        }
        /* printf("~ %d\n", pos); */
    }
    i_common = pos - i_start;
    i_len = i_common;
    pos = i_start - 1;

    /* remove ident from other lines */
    c_line = 2;
    while (pos < len)
    {
        /* loop for first (and each other) line lefts pos on newline char (\n), at end of code its not needn't */
        dst[d_pos++] = '\n';
        pos++;
        /* printf("| %d\n", pos); */

        c_start = pos;
        i_pos = 0;

        /* check for common indent */
        for (; pos < len && i_pos < i_common; pos++, i_pos++)
        {
            if (src[pos] != src[i_start + i_pos])
            {
                if (src[pos] == '\n')
                {
                    break;
                }
                if (src[pos] == ' ' || src[pos] == '\t')
                {
                    PyErr_Format(
                            PyExc_ValueError,
                            "inconsistent use of tabs and spaces in indentation (line %zd, pos %zd)",
                            c_line,
                            pos - c_start
                    );
                }
                else
                {
                    PyErr_Format(
                            PyExc_ValueError,
                            "too small common indent (line %zd, pos %zd)",
                            c_line,
                            pos - c_start
                    );
                }
                return -1;
            }
            /* printf("@~ %d\n", pos); */
        }
        if (src[pos] == '\n')
        {
            continue;
        }

        /* check for local indent */
        for (; pos < len && i_pos < i_len; pos++, i_pos++)
        {
            if (src[pos] != src[i_start + i_pos])
            {
                if (src[pos] == '\n')
                {
                    break;
                }
                if (src[pos] == ' ' || src[pos] == '\t')
                {
                    PyErr_Format(
                            PyExc_ValueError,
                            "inconsistent use of tabs and spaces in indentation (line %zd, pos %zd)",
                            c_line,
                            pos - c_start
                    );
                    return 1;
                }
                else
                {
                    break;
                }
            }
            dst[d_pos++] = src[pos];
            /* printf("@_ %d\n", pos); */
        }
        /* check for new indent level */
        while (pos < len && (src[pos] == ' ' || src[pos] == '\t'))
        {
            /*  printf("@+ %d\n", pos); */
            dst[d_pos++] = src[pos++];
        }
        if (src[pos] == '\n')
        {
            continue;
        }
        i_len = pos - c_start;
        i_start = c_start;

        /* copy statement */
        for (; pos < len && src[pos] != '\n'; pos++)
        {
            /* printf("@* %d '%c'\n", pos, src[pos]); */
            dst[d_pos++] = src[pos];
        }

        c_line++;
    }

    return d_pos;
}

PyHP_HIGH_API PyObject *PyHP_Core_AlignCode_Func(PyObject *module, PyObject *string)
{
    const char *src;
    char *dst;
    Py_ssize_t len;
    PyObject *new;

    if (!(PyUnicode_Check(string)))
    {
        PyErr_Format(PyExc_TypeError, "source must be str, not %s", Py_TYPE(string)->tp_name);
    }

    src = PyHP_Core_EncodeStringRO(string, &len);
    if (src == NULL)
    {
        return NULL;
    }

    dst = PyMem_Malloc(len);
    if (dst == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    len = PyHP_Core_AlignCode(dst, src, 0, len);
    if (len == -1)
    {
        PyMem_Free(dst);
        return NULL;
    }

    new = PyHP_Core_DecodeString(dst, 0, len);
    PyMem_Free(dst);
    if (new == NULL)
    {
        return NULL;
    }

    return new;
}

PyHP_HIGH_API PyHP_Core_ParserIterator_Object *PyHP_Core_Parser_Func(PyObject *module, PyObject *string)
{
    PyHP_Core_ParserIterator_Object *self;
    const char *src;
    Py_ssize_t len;

    if (!(PyUnicode_Check(string)))
    {
        PyErr_Format(PyExc_TypeError, "source must be str, not %s", Py_TYPE(string)->tp_name);
    }

    src = PyHP_Core_EncodeStringRO(string, &len);
    if (src == NULL)
    {
        return NULL;
    }

    self = (PyHP_Core_ParserIterator_Object *) (PyHP_Core_ParserIterator_Type.tp_alloc(&PyHP_Core_ParserIterator_Type, len));
    if (self == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    memcpy(self->buffer, src, len);
    PyHP_Core_Parser_Init(self->buffer, len, &self->state);

    return self;
}

PyHP_HIGH_API static PyHP_Core_ParserIterator_Object *PyHP_Core_ParserIterator_Iter(PyHP_Core_ParserIterator_Object *self)
{
    Py_INCREF(self);
    return self;
}

PyHP_HIGH_API static PyHP_Core_ParserMatch_Object *PyHP_Core_ParserIterator_Next(PyHP_Core_ParserIterator_Object *self)
{
    PyHP_Core_ParserMatch match;
    PyHP_Core_ParserMatch_Object *object;

    match = PyHP_Core_Parser_Next(self->buffer, Py_SIZE(self), &(self->state));

    if (match.type == PyHP_Core_StatementType_NONE)
    {
        return NULL;
    }

    object = (PyHP_Core_ParserMatch_Object *) (PyHP_Core_ParserMatch_Type.tp_alloc(&PyHP_Core_ParserMatch_Type, 0));
    if (object == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    object->meta = match;
    object->value = PyHP_Core_DecodeString(self->buffer, match.start, match.end - match.start);
    if (object->value == NULL)
    {
        Py_DECREF(object);
        return NULL;
    }

    return object;
}

PyHP_HIGH_API static void PyHP_Core_ParserIterator_Dealloc(PyHP_Core_ParserIterator_Object *self)
{
    Py_TYPE(self)->tp_free(self);
}

PyHP_HIGH_API static PyObject *PyHP_Core_ParserIterator_GetSource(PyHP_Core_ParserIterator_Object *self)
{
    return PyHP_Core_DecodeString(self->buffer, 0, Py_SIZE(self));
}

PyGetSetDef PyHP_Core_ParserIterator_GetSet[] = {
        {"_source", (getter) PyHP_Core_ParserIterator_GetSource, NULL, ""},
        {NULL}
};

PyTypeObject PyHP_Core_ParserIterator_Type = {
        PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "py3hp.core.parser_iterator",
        .tp_basicsize = sizeof(PyHP_Core_ParserIterator_Object) - sizeof(char[1]),
        .tp_itemsize = sizeof(char),
        .tp_dealloc = (destructor) PyHP_Core_ParserIterator_Dealloc,
        .tp_iter = (getiterfunc) PyHP_Core_ParserIterator_Iter,
        .tp_iternext = (iternextfunc) PyHP_Core_ParserIterator_Next,
        .tp_getset = PyHP_Core_ParserIterator_GetSet,
};


PyHP_HIGH_API static void PyHP_Core_ParserMatch_Dealloc(PyHP_Core_ParserMatch_Object *self)
{
    Py_XDECREF(self->value);
    Py_TYPE(self)->tp_free(self);
}

static PyMemberDef PyHP_Core_ParserMatch_Members[] = {
        {"value", T_OBJECT,   offsetof(PyHP_Core_ParserMatch_Object, value),      READONLY},
        {"start", T_PYSSIZET, offsetof(PyHP_Core_ParserMatch_Object, meta.start), READONLY},
        {"end",   T_PYSSIZET, offsetof(PyHP_Core_ParserMatch_Object, meta.end),   READONLY},
        {NULL}
};

static PyObject *PyHP_Core_ParserMatch_GetType(PyHP_Core_ParserMatch_Object *self)
{
    return PyLong_FromLong(self->meta.type);
}

static PyGetSetDef PyHP_Core_ParserMatch_GetSet[] = {
        {"type", (getter) PyHP_Core_ParserMatch_GetType, NULL, ""},
        {NULL}
};

PyHP_HIGH_API static PyObject *PyHP_Core_ParserMatch_Repr(PyHP_Core_ParserMatch_Object *self)
{
    return PyUnicode_FromFormat(
            "<%s object; span=(%zd, %zd) type=%d>",
            Py_TYPE(self)->tp_name,
            self->meta.start,
            self->meta.end,
            self->meta.type
    );
}

PyHP_HIGH_API static PyObject *PyHP_Core_ParserMatch_Str(PyHP_Core_ParserMatch_Object *self)
{
    Py_INCREF(self);
    return self->value;
}

PyTypeObject PyHP_Core_ParserMatch_Type = {
        PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "py3hp.core.parser_match",
        .tp_basicsize = sizeof(PyHP_Core_ParserMatch_Object),
        .tp_dealloc = (destructor) PyHP_Core_ParserMatch_Dealloc,
        .tp_repr = (reprfunc) PyHP_Core_ParserMatch_Repr,
        .tp_members = PyHP_Core_ParserMatch_Members,
        .tp_getset = PyHP_Core_ParserMatch_GetSet,
        /* .tp_str = (reprfunc) PyHP_Core_ParserMatch_Str, */
};

/* partially compatible code with python2x versions can be found on commit 4740e5a6e9d58405012dab74d54e82b76c7d2e1e */