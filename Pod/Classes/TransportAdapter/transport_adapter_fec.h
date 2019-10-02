
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http:
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJMEDIA_TRANSPORT_ADAPTER_FEC_H__
#define __PJMEDIA_TRANSPORT_ADAPTER_FEC_H__


/**
 * @file transport_adapter_fec.h
 * @brief FEC Media Transport Adapter
 */

#include <pjmedia/transport.h>
#include "lib_common/of_openfec_api.h"

/**
 * @defgroup PJMEDIA_TRANSPORT_ADAPTER_FEC FEC Transport Adapter
 * @ingroup PJMEDIA_TRANSPORT
 * @brief OpenFEC.org based FEC transport adapter.
 * @{
 *
 * This describes a implementation of FEC transport adapter, similar to
 * the way the SRTP transport adapter works.
 */

PJ_BEGIN_DECL


/**
 * Create the transport adapter, specifying the underlying transport to be
 * used to send and receive RTP/RTCP packets.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this media transport
 *			for logging purposes.
 * @param base_tp	The base/underlying media transport to send and
 * 			receive RTP/RTCP packets.
 * @param del_base	Specify whether the base transport should also be
 * 			destroyed when destroy() is called upon us.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_fec_adapter_create( pjmedia_endpt *endpt,
					        const char *name,
					        pjmedia_transport *base_tp,
					        pj_bool_t del_base,
						pjmedia_transport **p_tp);

PJ_END_DECL

/**
 * @}
 */

 /*
 * Simplified FEC Object Transmission Information structure, used to synchronize sender and receiver.
 *
 * NB: all the fields MUST be in Network Endian while sent over the network, so use htonl (resp. ntohl) at the sender (resp. receiver).
 */
	typedef struct {
	UINT32		codec_id;	/* identifies the code/codec being used. In practice, the "FEC encoding ID" that identifies the FEC Scheme should
							* be used instead (see [RFC5052]). In our example, we are not compliant with the RFCs anyway, so keep it simple. */
	UINT32		k;
	UINT32		n;
} fec_oti_t;


#endif	/* __PJMEDIA_TRANSPORT_ADAPTER_SAMPLE_H__ */


