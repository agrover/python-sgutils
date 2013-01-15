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

#define READCAP_RESP_BUFF_LEN 32

static char readcap_doc[] =
"Returns the result of READ CAPACITY(16), as a tuple.\n"
"See SCSI SBC-3 spec for more info:\n"
"(last_logical_block_address, logical_block_length, p_type, prot_en,\n"
" p_i_exponent, lbppbe, lbpme, lbprz, lalba)";

static PyObject *
sbc_readcap(PyObject *self, PyObject *args)
{
	int sg_fd;
	const char *sg_name;
	int ret;
	uint64_t llast_blk_addr, block_size;
	unsigned char resp_buff[READCAP_RESP_BUFF_LEN];
	int k, prot_en, p_type, lbpme, lbprz, lbppbe, lalba, p_i_exponent;

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(resp_buff, 0, sizeof(resp_buff));

	ret = sg_ll_readcap_16(sg_fd, 0, 0, &resp_buff, sizeof(resp_buff), 0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

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

	return Py_BuildValue("(kkbbbbbbb)", llast_blk_addr, block_size,
			     p_type, prot_en, p_i_exponent, lbppbe, lbpme,
			     lbprz, lalba);
}

static char inquiry_doc[] =
"Returns the result of INQUIRY, as a tuple.\n"
"See SCSI SPC-3 spec for more info:\n"
"(vendor, product, revision, peripheral_qualifier, peripheral_type,\n"
" rmb, version, NormACA, HiSup, response_data_format, sccs, acc,\n"
" tpgs, 3pc, protect, BQue, EncServ, MultiP, MChngr, Addr16, WBus16,\n"
" sync, linked, CmdQue)";

static PyObject *
spc_inquiry(PyObject *self, PyObject *args)
{
	int sg_fd;
	const char *sg_name;
	int ret;
	struct sg_simple_inquiry_resp inq_data;

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(&inq_data, 0, sizeof(inq_data));

	ret = sg_simple_inquiry(sg_fd, &inq_data, 0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

	return Py_BuildValue("(sssbbbbbbbbbbbbbbbbbbbbb)", inq_data.vendor,
			     inq_data.product, inq_data.revision,
			     inq_data.peripheral_qualifier,
			     inq_data.peripheral_type, inq_data.rmb,
			     inq_data.version,
			     ((inq_data.byte_3 >> 5) & 0x1),	/* NormACA */
			     ((inq_data.byte_3 >> 4) & 0x1),	/* HiSup */
			     (inq_data.byte_3 & 0xF),		/* response data format */
			     ((inq_data.byte_5 >> 7) & 0x1),	/* sccs */
			     ((inq_data.byte_5 >> 6) & 0x1),	/* acc */
			     ((inq_data.byte_5 >> 5) & 0x3),	/* tpgs */
			     ((inq_data.byte_5 >> 3) & 0x1),	/* 3pc */
			     (inq_data.byte_5 & 0x1),		/* protect */
			     ((inq_data.byte_6 >> 7) & 0x1),	/* BQue */
			     ((inq_data.byte_6 >> 6) & 0x1),	/* EncServ */
			     ((inq_data.byte_6 >> 4) & 0x1),	/* MultiP */
			     ((inq_data.byte_6 >> 3) & 0x1),	/* MChngr */
			     (inq_data.byte_6 & 0x1),		/* Addr16 */
			     ((inq_data.byte_7 >> 5) & 0x1),	/* WBus16 */
			     ((inq_data.byte_7 >> 4) & 0x1),	/* sync */
			     ((inq_data.byte_7 >> 3) & 0x1),	/* linked */
			     ((inq_data.byte_7 >> 1) & 0x1)	/* CmdQue */
		);
}

static char sense_doc[] =
"Returns the result of MODE SENSE(10), as a dictionary of sense mode codes\n"
" to page code data.\n"
"See SCSI SPC-3 spec for more info.";

#define PG_CODE_MAX 0x3f

static PyObject *
spc_sense(PyObject *self, PyObject *args)
{
	int sg_fd;
	const char *sg_name;
	int ret;
	unsigned char rbuf[4096];
	int i;
	unsigned char *ucp;
	unsigned int mode_data_len, block_desc_len;
	PyObject *dict;

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(&rbuf, 0, sizeof(rbuf));

	ret = sg_ll_mode_sense10(sg_fd, 0, 0, 0, 0x3f, 0, rbuf, sizeof(rbuf),
				 0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

	block_desc_len = (rbuf[6] << 8) + rbuf[7];
	/* rest_of_header_len = 6 */
	mode_data_len = (rbuf[0] << 8) + rbuf[1] - 6 - block_desc_len;
	/* 8 byte hdr, then block descriptors, THEN mode data */
	ucp = rbuf + 8 + block_desc_len;

	dict = PyDict_New();
	if (!dict)
		return NULL;

	/* Build a dict of sense mode page code to sense data */
	for (i = 0; mode_data_len > 0; i++) {
		PyObject *name, *value;
		int mp_len = ucp[1] + 2;

		name = PyInt_FromLong(ucp[0]);
		if (!name)
			goto py_err;

		value = PyString_FromStringAndSize((const char *)&ucp[2], ucp[1]);
		if (!value) {
			Py_DECREF(name);
			goto py_err;
		}

		if (PyDict_SetItem(dict, name, value) < 0) {
			Py_DECREF(name);
			Py_DECREF(value);
			goto py_err;
		}

		Py_DECREF(name);
		Py_DECREF(value);

		ucp += mp_len;
		mode_data_len -= mp_len;
	}

	return dict;

py_err:
	Py_XDECREF(dict);

	return NULL;
}

static PyMethodDef sgutils_methods[] = {
	{ "inquiry", spc_inquiry, METH_VARARGS, inquiry_doc},
	{ "sense", spc_sense, METH_VARARGS, sense_doc},
	{ "readcap", sbc_readcap, METH_VARARGS, readcap_doc},
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

