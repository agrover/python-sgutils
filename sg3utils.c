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
#include <scsi/sg_lib.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#define MX_ALLOC_LEN (0xc000 + 0x80)
static unsigned char rsp_buff[MX_ALLOC_LEN + 2];

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
	int k, prot_en, p_type, lbpme, lbprz, lbppbe, lalba, p_i_exponent;

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(rsp_buff, 0, sizeof(rsp_buff));

	ret = sg_ll_readcap_16(sg_fd, 0, 0, &rsp_buff, sizeof(rsp_buff), 0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

	for (k = 0, llast_blk_addr = 0; k < 8; ++k) {
		llast_blk_addr <<= 8;
		llast_blk_addr |= rsp_buff[k];
	}
	block_size = ((rsp_buff[8] << 24) | (rsp_buff[9] << 16) |
		      (rsp_buff[10] << 8) | rsp_buff[11]);
	p_type = ((rsp_buff[12] >> 1) & 0x7);
	prot_en = !!(rsp_buff[12] & 0x1);
	p_i_exponent = (rsp_buff[13] >> 4);
	lbppbe = rsp_buff[13] & 0xf;
	lbpme = !!(rsp_buff[14] & 0x80);
	lbprz = !!(rsp_buff[14] & 0x40);
	lalba = ((rsp_buff[14] & 0x3f) << 8) + rsp_buff[15];

	return Py_BuildValue("(kkbbbbbbb)", llast_blk_addr, block_size,
			     p_type, prot_en, p_i_exponent, lbppbe, lbpme,
			     lbprz, lalba);
}

static PyObject *
spc_simple_inquiry(const char *sg_name)
{
	int sg_fd;
	int ret;
	struct sg_simple_inquiry_resp inq_data;

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

static PyObject *
spc_inq_0x80(const char *sg_name)
{
	int ret;
	int sg_fd;

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(&rsp_buff, 0, sizeof(rsp_buff));

	ret = sg_ll_inquiry(sg_fd, 0, 1, 0x80, rsp_buff, sizeof(rsp_buff),
			    0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

	return Py_BuildValue("s#", &rsp_buff[4], rsp_buff[3]);
}

static const char * assoc_arr[] =
{
    "Addressed logical unit",
    "Target port",      /* that received request; unless SCSI ports VPD */
    "Target device that contains addressed lu",
    "Reserved [0x3]",
};

static const char * code_set_arr[] =
{
    "Reserved [0x0]",
    "Binary",
    "ASCII",
    "UTF-8",
    "Reserved [0x4]", "Reserved [0x5]", "Reserved [0x6]", "Reserved [0x7]",
    "Reserved [0x8]", "Reserved [0x9]", "Reserved [0xa]", "Reserved [0xb]",
    "Reserved [0xc]", "Reserved [0xd]", "Reserved [0xe]", "Reserved [0xf]",
};

static const char * desig_type_arr[] =
{
    "vendor specific [0x0]",
    "T10 vendor identification",
    "EUI-64 based",
    "NAA",
    "Relative target port",
    "Target port group",        /* spc4r09: _primary_ target port group */
    "Logical unit group",
    "MD5 logical unit identifier",
    "SCSI name string",
    "Protocol specific port identifier",  /* spc4r36 */
    "Reserved [0xa]", "Reserved [0xb]",
    "Reserved [0xc]", "Reserved [0xd]", "Reserved [0xe]", "Reserved [0xf]",
};

static PyObject *
decode_vpd83_descriptor(const unsigned char * ip, int i_len,
			int p_id, int c_set, int piv, int assoc,
			int desig_type, int long_out, int print_assoc)
{
	char b[64];
	PyObject *tuple;

	tuple = PyTuple_New(5);
	if (!tuple)
		return NULL;

	PyTuple_SetItem(tuple, 0, PyString_FromString(assoc_arr[assoc]));
	PyTuple_SetItem(tuple, 1, PyString_FromString(desig_type_arr[desig_type]));
	PyTuple_SetItem(tuple, 2, PyString_FromString(code_set_arr[c_set]));
	if (piv && ((1 == assoc) || (2 == assoc))) {
		sg_get_trans_proto_str(p_id, sizeof(b), b);
		PyTuple_SetItem(tuple, 3, PyString_FromString(b));
	} else {
		PyTuple_SetItem(tuple, 3, PyString_FromString("N/A"));
	}

	PyTuple_SetItem(tuple, 4, Py_BuildValue("s#", ip, i_len));

	return tuple;
}

static PyObject *
spc_inq_0x83(const char *sg_name)
{
	int ret;
	int sg_fd;
	int assoc, i_len, c_set, piv, p_id, desig_type;
	int off, u;
	const unsigned char * ucp;
	int len;
	unsigned char *id_buff;
	PyObject *list;

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(&rsp_buff, 0, sizeof(rsp_buff));

	ret = sg_ll_inquiry(sg_fd, 0, 1, 0x83, rsp_buff, sizeof(rsp_buff),
			    0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

	len = ((rsp_buff[2] << 8) + rsp_buff[3]) + 4;
	if (len > sizeof(rsp_buff)) {
		PyErr_SetString(PyExc_IOError, "Return data too long");
		return NULL;
	}

	list = PyList_New(0);

	off = -1;
	id_buff = rsp_buff + 4;
	
	while ((u = sg_vpd_dev_id_iter(id_buff, (len - 4), &off, -1, -1, -1)) == 0) {
		PyObject *desc;

		ucp = id_buff + off;
		i_len = ucp[3];
		if ((off + i_len + 4) > len) {
			PyErr_SetString(PyExc_IOError,
				"VPD page error: designator length longer "
				"than remaining response length");
			return NULL;
		}

		assoc = ((ucp[1] >> 4) & 0x3);
		p_id = ((ucp[0] >> 4) & 0xf);
		c_set = (ucp[0] & 0xf);
		piv = ((ucp[1] & 0x80) ? 1 : 0);
		desig_type = (ucp[1] & 0xf);
		desc = decode_vpd83_descriptor(ucp + 4, i_len, p_id, c_set,
					       piv, assoc, desig_type, 0, 0);
		if (!desc) {
			Py_DECREF(list);
			return NULL;
		}

		if (PyList_Append(list, desc)) {
			Py_DECREF(list);
			return NULL;
		}
	}

	return list;
}

static char inquiry_doc[] =
"Returns the result of INQUIRY.\n"
"See SCSI SPC-3 spec for more info:\n"
"Called with a single devname argument, returns a tuple of basic INQUIRY:\n"
"Example: inquiry(\"/dev/sda\") ->\n"
"(vendor, product, revision, peripheral_qualifier, peripheral_type,\n"
" rmb, version, NormACA, HiSup, response_data_format, sccs, acc,\n"
" tpgs, 3pc, protect, BQue, EncServ, MultiP, MChngr, Addr16, WBus16,\n"
" sync, linked, CmdQue)\n\n"
"Called with devname and page arguments, returns tuple of that page's\n"
" information.\n"
"Example: inquiry(\"/dev/sda\", 0x80)\n"
"Currently supported pages:\n\n"
"0x80: Unit serial number\n"
"  Returns a string of the unit serial number.\n\n"
"0x83: Device information\n"
"  Returns a list of identification descriptors.\n"
"  Each element is a tuple containing association, identifier type,\n"
"  code set, transport protocol identifier or \"N/A\", and the identifier\n"
"  itself as a string.\n";

static PyObject *
spc_inquiry(PyObject *self, PyObject *args)
{
	const char *sg_name;
	int page = 0;

	if (!PyArg_ParseTuple(args, "s|i", &sg_name, &page)) {
		return NULL;
	}

	switch (page) {
	case 0:
		return spc_simple_inquiry(sg_name);
	case 0x80:
		return spc_inq_0x80(sg_name);
	case 0x83:
		return spc_inq_0x83(sg_name);
	default:
		PyErr_SetString(PyExc_IOError, "Unsupported VPD page");
		return NULL;
	}
}

static char mode_sense_doc[] =
"Returns the result of MODE SENSE(10), as a dictionary of sense mode codes\n"
" to page code data.\n"
"See SCSI SPC-3 spec for more info.";

#define PG_CODE_MAX 0x3f

static PyObject *
spc_mode_sense(PyObject *self, PyObject *args)
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


static char report_luns_doc[] =
"Returns the result of REPORT LUNS.\n"
"Currently only non-hierarchical LUNs are supported.\n"
"See SCSI SPC-3 and SAM-5 specs for more info.";

#define REPORT_LUNS_MAX_COUNT 256
#define LUN_SIZE 8

static PyObject *
spc_report_luns(PyObject *self, PyObject *args)
{
	int sg_fd;
	const char *sg_name;
	int ret;
	unsigned char rbuf[REPORT_LUNS_MAX_COUNT * LUN_SIZE];
	int i;
	PyObject *tuple;
	int luns;
	int off;

	if (!PyArg_ParseTuple(args, "s", &sg_name)) {
		return NULL;
	}

	sg_fd = sg_cmds_open_device(sg_name, 1, 0);
	if (sg_fd < 0) {
		PyErr_SetString(PyExc_IOError, "Could not open device");
		return NULL;
	}

	memset(&rbuf, 0, sizeof(rbuf));

	/* only report=0 for now */
	ret = sg_ll_report_luns(sg_fd, 0, rbuf, sizeof(rbuf), 0, 0);
	if (ret < 0) {
		PyErr_SetString(PyExc_IOError, "SCSI command failed");
		sg_cmds_close_device(sg_fd);
		return NULL;
	}

	sg_cmds_close_device(sg_fd);

	luns = ((rbuf[0] << 24) + (rbuf[1] << 16) +
		(rbuf[2] << 8) + rbuf[3]) / LUN_SIZE;

	tuple = PyTuple_New(luns);
	if (!tuple)
		return NULL;

	/* 
	 * Currently only support non-hierarchical LUNs up to 256.
	 * In this (easy) case, LUN is at byte 1 in the 8-bytes.
	 */
	for (i = 0, off = 8; i < luns; i++, off += LUN_SIZE) {
		int lun;

		lun = rbuf[off+1];

		PyTuple_SET_ITEM(tuple, i, PyInt_FromLong(lun));
	}

	return tuple;
}

static PyMethodDef sgutils_methods[] = {
	{ "inquiry", spc_inquiry, METH_VARARGS, inquiry_doc},
	{ "mode_sense", spc_mode_sense, METH_VARARGS, mode_sense_doc},
	{ "readcap", sbc_readcap, METH_VARARGS, readcap_doc},
	{ "report_luns", spc_report_luns, METH_VARARGS, report_luns_doc},
#if 0
	/* TODO */
	{ "format_unit", x, METH_VARARGS, x}, /* for DIF support */
	{ "persistent_reserve", x, METH_VARARGS, x},
	{ "unmap", sbc_unmap, METH_VARARGS, unmap_doc}, /* use v2? */
	/* newer stuff */
	{ "extended_copy", x, METH_VARARGS, x},
	{ "receive_copy_results", x, METH_VARARGS, x},
#endif
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

