/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2017, 2018, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/trx0rseg.h
Rollback segment

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rseg_h
#define trx0rseg_h

#include "trx0sys.h"
#include "fut0lst.h"

/** Gets a rollback segment header.
@param[in]	space		space where placed
@param[in]	page_no		page number of the header
@param[in,out]	mtr		mini-transaction
@return rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get(
	ulint			space,
	ulint			page_no,
	mtr_t*			mtr);

/** Gets a newly created rollback segment header.
@param[in]	space		space where placed
@param[in]	page_no		page number of the header
@param[in,out]	mtr		mini-transaction
@return rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t*
trx_rsegf_get_new(
	ulint			space,
	ulint			page_no,
	mtr_t*			mtr);

/***************************************************************//**
Sets the file page number of the nth undo log slot. */
UNIV_INLINE
void
trx_rsegf_set_nth_undo(
/*===================*/
	trx_rsegf_t*	rsegf,	/*!< in: rollback segment header */
	ulint		n,	/*!< in: index of slot */
	ulint		page_no,/*!< in: page number of the undo log segment */
	mtr_t*		mtr);	/*!< in: mtr */
/****************************************************************//**
Looks for a free slot for an undo log segment.
@return slot index or ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
trx_rsegf_undo_find_free(const trx_rsegf_t* rsegf);

/** Creates a rollback segment header.
This function is called only when a new rollback segment is created in
the database.
@param[in]	space		space id
@param[in]	rseg_id		rollback segment identifier
@param[in,out]	sys_header	the TRX_SYS page (NULL for temporary rseg)
@param[in,out]	mtr		mini-transaction
@return page number of the created segment, FIL_NULL if fail */
ulint
trx_rseg_header_create(
	ulint			space,
	ulint			rseg_id,
	buf_block_t*		sys_header,
	mtr_t*			mtr);

/** Initialize the rollback segments in memory at database startup. */
void
trx_rseg_array_init();

/** Free a rollback segment in memory. */
void
trx_rseg_mem_free(trx_rseg_t* rseg);

/** Create a persistent rollback segment.
@param[in]	space_id	system or undo tablespace id
@return pointer to new rollback segment
@retval	NULL	on failure */
trx_rseg_t*
trx_rseg_create(ulint space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Create the temporary rollback segments. */
void
trx_temp_rseg_create();

/********************************************************************
Get the number of unique rollback tablespaces in use except space id 0.
The last space id will be the sentinel value ULINT_UNDEFINED. The array
will be sorted on space id. Note: space_ids should have have space for
TRX_SYS_N_RSEGS + 1 elements.
@return number of unique rollback tablespaces in use. */
ulint
trx_rseg_get_n_undo_tablespaces(
/*============================*/
	ulint*		space_ids);	/*!< out: array of space ids of
					UNDO tablespaces */
/* Number of undo log slots in a rollback segment file copy */
#define TRX_RSEG_N_SLOTS	(UNIV_PAGE_SIZE / 16)

/* Maximum number of transactions supported by a single rollback segment */
#define TRX_RSEG_MAX_N_TRXS	(TRX_RSEG_N_SLOTS / 2)

/** The rollback segment memory object */
struct trx_rseg_t {
	/*--------------------------------------------------------*/
	/** rollback segment id == the index of its slot in the trx
	system file copy */
	ulint				id;

	/** mutex protecting the fields in this struct except id,space,page_no
	which are constant */
	RsegMutex			mutex;

	/** space where the rollback segment header is placed */
	ulint				space;

	/** page number of the rollback segment header */
	ulint				page_no;

	/** current size in pages */
	ulint				curr_size;

	/*--------------------------------------------------------*/
	/* Fields for undo logs */
	/** List of undo logs */
	UT_LIST_BASE_NODE_T(trx_undo_t)	undo_list;

	/** List of undo log segments cached for fast reuse */
	UT_LIST_BASE_NODE_T(trx_undo_t)	undo_cached;

	/** List of recovered old insert_undo logs of incomplete
	transactions (to roll back or XA COMMIT & purge) */
	UT_LIST_BASE_NODE_T(trx_undo_t) old_insert_list;

	/*--------------------------------------------------------*/

	/** Page number of the last not yet purged log header in the history
	list; FIL_NULL if all list purged */
	ulint				last_page_no;

	/** Byte offset of the last not yet purged log header */
	ulint				last_offset;

	/** Transaction number of the last not yet purged log */
	trx_id_t			last_trx_no;

	/** Whether the log segment needs purge */
	bool				needs_purge;

	/** Reference counter to track rseg allocated transactions. */
	ulint				trx_ref_count;

	/** If true, then skip allocating this rseg as it reside in
	UNDO-tablespace marked for truncate. */
	bool				skip_allocation;

	/** @return whether the rollback segment is persistent */
	bool is_persistent() const
	{
		ut_ad(space == SRV_TMP_SPACE_ID
		      || space == TRX_SYS_SPACE
		      || (srv_undo_space_id_start > 0
			  && space >= srv_undo_space_id_start
			  && space <= srv_undo_space_id_start
			  + TRX_SYS_MAX_UNDO_SPACES));
		ut_ad(space == SRV_TMP_SPACE_ID
		      || space == TRX_SYS_SPACE
		      || (srv_undo_space_id_start > 0
			  && space >= srv_undo_space_id_start
			  && space <= srv_undo_space_id_start
			  + srv_undo_tablespaces_active)
		      || !srv_was_started);
		return(space != SRV_TMP_SPACE_ID);
	}
};

/* Undo log segment slot in a rollback segment header */
/*-------------------------------------------------------------*/
#define	TRX_RSEG_SLOT_PAGE_NO	0	/* Page number of the header page of
					an undo log segment */
/*-------------------------------------------------------------*/
/* Slot size */
#define TRX_RSEG_SLOT_SIZE	4

/* The offset of the rollback segment header on its page */
#define	TRX_RSEG		FSEG_PAGE_DATA

/* Transaction rollback segment header */
/*-------------------------------------------------------------*/
/** 0xfffffffe = pre-MariaDB 10.3.5 format; 0=MariaDB 10.3.5 or later */
#define	TRX_RSEG_FORMAT		0
/** Number of pages in the TRX_RSEG_HISTORY list */
#define	TRX_RSEG_HISTORY_SIZE	4
/** Committed transaction logs that have not been purged yet */
#define	TRX_RSEG_HISTORY	8
#define	TRX_RSEG_FSEG_HEADER	(8 + FLST_BASE_NODE_SIZE)
					/* Header for the file segment where
					this page is placed */
#define TRX_RSEG_UNDO_SLOTS	(8 + FLST_BASE_NODE_SIZE + FSEG_HEADER_SIZE)
					/* Undo log segment slots */
/** Maximum transaction ID (valid only if TRX_RSEG_FORMAT is 0) */
#define TRX_RSEG_MAX_TRX_ID	(TRX_RSEG_UNDO_SLOTS + TRX_RSEG_N_SLOTS	\
				 * TRX_RSEG_SLOT_SIZE)

/** 8 bytes offset within the binlog file */
#define TRX_RSEG_BINLOG_OFFSET		TRX_RSEG_MAX_TRX_ID + 8
/** MySQL log file name, 512 bytes, including terminating NUL
(valid only if TRX_RSEG_FORMAT is 0).
If no binlog information is present, the first byte is NUL. */
#define TRX_RSEG_BINLOG_NAME		TRX_RSEG_MAX_TRX_ID + 16
/** Maximum length of binlog file name, including terminating NUL, in bytes */
#define TRX_RSEG_BINLOG_NAME_LEN	512

#ifdef WITH_WSREP
/** The offset to WSREP XID headers */
#define	TRX_RSEG_WSREP_XID_INFO		TRX_RSEG_MAX_TRX_ID + 16 + 512

/** WSREP XID format (1 if present and valid, 0 if not present) */
#define TRX_RSEG_WSREP_XID_FORMAT	TRX_RSEG_WSREP_XID_INFO
/** WSREP XID GTRID length */
#define TRX_RSEG_WSREP_XID_GTRID_LEN	TRX_RSEG_WSREP_XID_INFO + 4
/** WSREP XID bqual length */
#define TRX_RSEG_WSREP_XID_BQUAL_LEN	TRX_RSEG_WSREP_XID_INFO + 8
/** WSREP XID data (XIDDATASIZE bytes) */
#define TRX_RSEG_WSREP_XID_DATA		TRX_RSEG_WSREP_XID_INFO + 12
#endif /* WITH_WSREP*/

/*-------------------------------------------------------------*/

/** Read the page number of an undo log slot.
@param[in]	rsegf	rollback segment header
@param[in]	n	slot number */
inline
uint32_t
trx_rsegf_get_nth_undo(const trx_rsegf_t* rsegf, ulint n)
{
	ut_ad(n < TRX_RSEG_N_SLOTS);
	return mach_read_from_4(rsegf + TRX_RSEG_UNDO_SLOTS
				+ n * TRX_RSEG_SLOT_SIZE);
}

#ifdef WITH_WSREP
/** Update the WSREP XID information in rollback segment header.
@param[in,out]	rseg_header	rollback segment header
@param[in]	xid		WSREP XID
@param[in,out]	mtr		mini-transaction */
void
trx_rseg_update_wsrep_checkpoint(
	trx_rsegf_t*	rseg_header,
	const XID*	xid,
	mtr_t*		mtr);

/** Update WSREP checkpoint XID in first rollback segment header.
@param[in]	xid		WSREP XID */
void trx_rseg_update_wsrep_checkpoint(const XID* xid);

/** Read the WSREP XID information in rollback segment header.
@param[in]	rseg_header	Rollback segment header
@param[out]	xid		Transaction XID
@return	whether the WSREP XID was present */
bool trx_rseg_read_wsrep_checkpoint(const trx_rsegf_t* rseg_header, XID& xid);

/** Recover the latest WSREP checkpoint XID.
@param[out]	xid	WSREP XID
@return	whether the WSREP XID was found */
bool trx_rseg_read_wsrep_checkpoint(XID& xid);
#endif /* WITH_WSREP */

/** Upgrade a rollback segment header page to MariaDB 10.3 format.
@param[in,out]	rseg_header	rollback segment header page
@param[in,out]	mtr		mini-transaction */
void trx_rseg_format_upgrade(trx_rsegf_t* rseg_header, mtr_t* mtr);

/** Update the offset information about the end of the binlog entry
which corresponds to the transaction just being committed.
In a replication slave, this updates the master binlog position
up to which replication has proceeded.
@param[in,out]	rseg_header	rollback segment header
@param[in]	trx		committing transaction
@param[in,out]	mtr		mini-transaction */
void
trx_rseg_update_binlog_offset(byte* rseg_header, const trx_t* trx, mtr_t* mtr);

#include "trx0rseg.ic"

#endif
