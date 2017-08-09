/* 
   Unix SMB/CIFS implementation.
   ads sasl wrapping code
   Copyright (C) Stefan Metzmacher 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "ads.h"

#ifdef HAVE_LDAP_SASL_WRAPPING

static int ads_saslwrap_setup(Sockbuf_IO_Desc *sbiod, void *arg)
{
	ADS_STRUCT *ads = (ADS_STRUCT *)arg;

	ads->ldap.sbiod	= sbiod;

	sbiod->sbiod_pvt = ads;

	return 0;
}

static int ads_saslwrap_remove(Sockbuf_IO_Desc *sbiod)
{
	return 0;
}

static ber_slen_t ads_saslwrap_prepare_inbuf(ADS_STRUCT *ads)
{
	ads->ldap.in.ofs	= 0;
	ads->ldap.in.needed	= 0;
	ads->ldap.in.left	= 0;
	ads->ldap.in.size	= 4 + ads->ldap.in.min_wrapped;
	ads->ldap.in.buf	= talloc_array(ads->ldap.mem_ctx,
					       uint8, ads->ldap.in.size);
	if (!ads->ldap.in.buf) {
		return -1;
	}

	return 0;
}

static ber_slen_t ads_saslwrap_grow_inbuf(ADS_STRUCT *ads)
{
	if (ads->ldap.in.size == (4 + ads->ldap.in.needed)) {
		return 0;
	}

	ads->ldap.in.size	= 4 + ads->ldap.in.needed;
	ads->ldap.in.buf	= talloc_realloc(ads->ldap.mem_ctx,
						 ads->ldap.in.buf,
						 uint8, ads->ldap.in.size);
	if (!ads->ldap.in.buf) {
		return -1;
	}

	return 0;
}

static void ads_saslwrap_shrink_inbuf(ADS_STRUCT *ads)
{
	talloc_free(ads->ldap.in.buf);

	ads->ldap.in.buf	= NULL;
	ads->ldap.in.size	= 0;
	ads->ldap.in.ofs	= 0;
	ads->ldap.in.needed	= 0;
	ads->ldap.in.left	= 0;
}

static ber_slen_t ads_saslwrap_read(Sockbuf_IO_Desc *sbiod, void *buf, ber_len_t len)
{
	ADS_STRUCT *ads = (ADS_STRUCT *)sbiod->sbiod_pvt;
	ber_slen_t ret;

	/* If ofs < 4 it means we don't have read the length header yet */
	if (ads->ldap.in.ofs < 4) {
		ret = ads_saslwrap_prepare_inbuf(ads);
		if (ret < 0) return ret;

		ret = LBER_SBIOD_READ_NEXT(sbiod,
					   ads->ldap.in.buf + ads->ldap.in.ofs,
					   4 - ads->ldap.in.ofs);
		if (ret <= 0) return ret;
		ads->ldap.in.ofs += ret;

		if (ads->ldap.in.ofs < 4) goto eagain;

		ads->ldap.in.needed = RIVAL(ads->ldap.in.buf, 0);
		if (ads->ldap.in.needed > ads->ldap.in.max_wrapped) {
			errno = EINVAL;
			return -1;
		}
		if (ads->ldap.in.needed < ads->ldap.in.min_wrapped) {
			errno = EINVAL;
			return -1;
		}

		ret = ads_saslwrap_grow_inbuf(ads);
		if (ret < 0) return ret;
	}

	/*
	 * if there's more data needed from the remote end,
	 * we need to read more
	 */
	if (ads->ldap.in.needed > 0) {
		ret = LBER_SBIOD_READ_NEXT(sbiod,
					   ads->ldap.in.buf + ads->ldap.in.ofs,
					   ads->ldap.in.needed);
		if (ret <= 0) return ret;
		ads->ldap.in.ofs += ret;
		ads->ldap.in.needed -= ret;

		if (ads->ldap.in.needed > 0) goto eagain;
	}

	/*
	 * if we have a complete packet and have not yet unwrapped it
	 * we need to call the mech specific unwrap() hook
	 */
	if (ads->ldap.in.needed == 0 && ads->ldap.in.left == 0) {
		ADS_STATUS status;
		status = ads->ldap.wrap_ops->unwrap(ads);
		if (!ADS_ERR_OK(status)) {
			errno = EACCES;
			return -1;
		}
	}

	/*
	 * if we have unwrapped data give it to the caller
	 */
	if (ads->ldap.in.left > 0) {
		ret = MIN(ads->ldap.in.left, len);
		memcpy(buf, ads->ldap.in.buf + ads->ldap.in.ofs, ret);
		ads->ldap.in.ofs += ret;
		ads->ldap.in.left -= ret;

		/*
		 * if no more is left shrink the inbuf,
		 * this will trigger reading a new SASL packet
		 * from the remote stream in the next call
		 */
		if (ads->ldap.in.left == 0) {
			ads_saslwrap_shrink_inbuf(ads);
		}

		return ret;
	}

	/*
	 * if we don't have anything for the caller yet,
	 * tell him to ask again
	 */
eagain:
	errno = EAGAIN;
	return -1;
}

static ber_slen_t ads_saslwrap_prepare_outbuf(ADS_STRUCT *ads, uint32 len)
{
	ads->ldap.out.ofs	= 0;
	ads->ldap.out.left	= 0;
	ads->ldap.out.size	= 4 + ads->ldap.out.sig_size + len;
	ads->ldap.out.buf	= talloc_array(ads->ldap.mem_ctx,
					       uint8, ads->ldap.out.size);
	if (!ads->ldap.out.buf) {
		return -1;
	}

	return 0;
}

static void ads_saslwrap_shrink_outbuf(ADS_STRUCT *ads)
{
	talloc_free(ads->ldap.out.buf);

	ads->ldap.out.buf	= NULL;
	ads->ldap.out.size	= 0;
	ads->ldap.out.ofs	= 0;
	ads->ldap.out.left	= 0;
}

static ber_slen_t ads_saslwrap_write(Sockbuf_IO_Desc *sbiod, void *buf, ber_len_t len)
{
	ADS_STRUCT *ads = (ADS_STRUCT *)sbiod->sbiod_pvt;
	ber_slen_t ret, rlen;

	/* if the buffer is empty, we need to wrap in incoming buffer */
	if (ads->ldap.out.left == 0) {
		ADS_STATUS status;

		if (len == 0) {
			errno = EINVAL;
			return -1;
		}

		rlen = MIN(len, ads->ldap.out.max_unwrapped);

		ret = ads_saslwrap_prepare_outbuf(ads, rlen);
		if (ret < 0) return ret;
		
		status = ads->ldap.wrap_ops->wrap(ads, (uint8 *)buf, rlen);
		if (!ADS_ERR_OK(status)) {
			errno = EACCES;
			return -1;
		}

		RSIVAL(ads->ldap.out.buf, 0, ads->ldap.out.left - 4);
	} else {
		rlen = -1;
	}

	ret = LBER_SBIOD_WRITE_NEXT(sbiod,
				    ads->ldap.out.buf + ads->ldap.out.ofs,
				    ads->ldap.out.left);
	if (ret <= 0) return ret;
	ads->ldap.out.ofs += ret;
	ads->ldap.out.left -= ret;

	if (ads->ldap.out.left == 0) {
		ads_saslwrap_shrink_outbuf(ads);
	}

	if (rlen > 0) return rlen;

	errno = EAGAIN;
	return -1;
}

static int ads_saslwrap_ctrl(Sockbuf_IO_Desc *sbiod, int opt, void *arg)
{
	ADS_STRUCT *ads = (ADS_STRUCT *)sbiod->sbiod_pvt;
	int ret;

	switch (opt) {
	case LBER_SB_OPT_DATA_READY:
		if (ads->ldap.in.left > 0) {
			return 1;
		}
		ret = LBER_SBIOD_CTRL_NEXT(sbiod, opt, arg);
		break;
	default:
		ret = LBER_SBIOD_CTRL_NEXT(sbiod, opt, arg);
		break;
	}

	return ret;
}

static int ads_saslwrap_close(Sockbuf_IO_Desc *sbiod)
{
	return 0;
}

static const Sockbuf_IO ads_saslwrap_sockbuf_io = {
	ads_saslwrap_setup,	/* sbi_setup */
	ads_saslwrap_remove,	/* sbi_remove */
	ads_saslwrap_ctrl,	/* sbi_ctrl */
	ads_saslwrap_read,	/* sbi_read */
	ads_saslwrap_write,	/* sbi_write */
	ads_saslwrap_close	/* sbi_close */
};

ADS_STATUS ads_setup_sasl_wrapping(ADS_STRUCT *ads,
				   const struct ads_saslwrap_ops *ops,
				   void *private_data)
{
	ADS_STATUS status;
	Sockbuf *sb;
	Sockbuf_IO *io = discard_const_p(Sockbuf_IO, &ads_saslwrap_sockbuf_io);
	int rc;

	rc = ldap_get_option(ads->ldap.ld, LDAP_OPT_SOCKBUF, &sb);
	status = ADS_ERROR_LDAP(rc);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	/* setup the real wrapping callbacks */
	rc = ber_sockbuf_add_io(sb, io, LBER_SBIOD_LEVEL_TRANSPORT, ads);
	status = ADS_ERROR_LDAP(rc);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	ads->ldap.wrap_ops		= ops;
	ads->ldap.wrap_private_data	= private_data;

	return ADS_SUCCESS;
}
#else
ADS_STATUS ads_setup_sasl_wrapping(ADS_STRUCT *ads,
				   const struct ads_saslwrap_ops *ops,
				   void *private_data)
{
	return ADS_ERROR_NT(NT_STATUS_NOT_SUPPORTED);
}
#endif /* HAVE_LDAP_SASL_WRAPPING */
