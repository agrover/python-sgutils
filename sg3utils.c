/*
 * python-sgutils -- Python interface to sg3_utils' libsgutils.
 *
 * Copyright (C) 2013 Red Hat, Inc.
 * Copyright (C) 1999-2011 Douglas Gilbert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Andy Grover (agrover redhat com)
 */

#include <Python.h>
#include <fcntl.h>
#include <scsi/sg_cmds.h>

/* Returns >= 0 if successful, otherwise returns negated errno. */
static int open_device(const char *device_name)
{
	int fd;
	int oflags = O_NONBLOCK | O_RDONLY;

	fd = open(device_name, oflags);
	if (fd < 0)
		fd = -errno;
	return fd;
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int close_device(int device_fd)
{
	int res;

	res = close(device_fd);
	if (res < 0)
		res = -errno;
	return res;
}

#define RESP_BUFF_LEN 8

static PyObject *
readcap_10(PyObject *self, PyObject *args)
{
	int sg_fd;
	const char *sg_name;
	int ret;
	char resp_buff[RESP_BUFF_LEN];

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = open_device(sg_name);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(resp_buff, 0, sizeof(resp_buff));

	ret = sg_ll_readcap_10(sg_fd, 0, 0, &resp_buff, RESP_BUFF_LEN, 0, 0);
	if (ret == 0)
		printf("yay!\n");
	else
		printf("boo! %d\n", ret);

	close_device(sg_fd);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef sgutils_methods[] = {
	{ "readcap_10",		(PyCFunction)readcap_10, METH_VARARGS },
	{ NULL,	     NULL}	   /* sentinel */
};


PyMODINIT_FUNC
initsgutils(void)
{
	PyObject *m;

	m = Py_InitModule3("sgutils", sgutils_methods, "sgutils module");
	if (m == NULL)
		return;
}

