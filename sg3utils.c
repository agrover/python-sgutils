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
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

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

#define RESP_BUFF_LEN 32

static char readcap_doc[] =
"Returns the result of READ CAPACITY(16), as a tuple.\n"
"See SCSI SBC-3 spec for more info:\n"
"(last_logical_block_address, logical_block_length, p_type, prot_en,\n"
" p_i_exponent, lbppbe, lbpme, lbprz, lalba)";

static PyObject *
readcap(PyObject *self, PyObject *args)
{
	int sg_fd;
	const char *sg_name;
	int ret;
	uint64_t llast_blk_addr, block_size;
	unsigned char resp_buff[RESP_BUFF_LEN];
	int k, prot_en, p_type, lbpme, lbprz, lbppbe, lalba, p_i_exponent;

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = open_device(sg_name);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(resp_buff, 0, sizeof(resp_buff));

	ret = sg_ll_readcap_16(sg_fd, 0, 0, &resp_buff, RESP_BUFF_LEN, 0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		close_device(sg_fd);
		return NULL;
	}

	close_device(sg_fd);

	for (k = 0, llast_blk_addr = 0; k < 8; ++k) {
		llast_blk_addr <<= 8;
		llast_blk_addr |= resp_buff[k];
	}
	block_size = ((resp_buff[8] << 24) | (resp_buff[9] << 16) |
		      (resp_buff[10] << 8) | resp_buff[11]);
	p_type = ((resp_buff[12] >> 1) & 0x7);
	prot_en = !!(resp_buff[12] & 0x1);
	p_i_exponent = (resp_buff[13] >> 4);
	lbppbe = resp_buff[13] & 0xf;
	lbpme = !!(resp_buff[14] & 0x80);
	lbprz = !!(resp_buff[14] & 0x40);
	lalba = ((resp_buff[14] & 0x3f) << 8) + resp_buff[15];

	return Py_BuildValue("(kkkkkkkkk)", llast_blk_addr, block_size,
			     p_type, prot_en, p_i_exponent, lbppbe, lbpme,
			     lbprz, lalba);
}

static PyMethodDef sgutils_methods[] = {
	{ "readcap",	readcap, METH_VARARGS,
	readcap_doc},
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

