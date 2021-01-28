/* Copyright (C) 2020 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \ingroup decode
 *
 * @{
 */


/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 *
 * Decodes ERSPAN Types I and II
 */

#include "suricata-common.h"
#include "suricata.h"
#include "decode.h"
#include "decode-events.h"
#include "decode-erspan.h"

#include "util-unittest.h"
#include "util-debug.h"

/**
 * \brief Functions to decode ERSPAN Type I and II packets
 */

bool g_erspan_typeI_enabled = false;

void DecodeERSPANConfig(void)
{
    int enabled = 0;
    if (ConfGetBool("decoder.erspan.typeI.enabled", &enabled) == 1) {
        g_erspan_typeI_enabled = (enabled == 1);
    }
    SCLogDebug("ERSPAN Type I decode support %s", g_erspan_typeI_enabled ? "enabled" : "disabled");
}

/**
 * \brief ERSPAN Type I
 */
int DecodeERSPANTypeI(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p,
                      const uint8_t *pkt, uint32_t len, PacketQueue *pq)
{
    if (unlikely(!g_erspan_typeI_enabled))
        return TM_ECODE_FAILED;

    StatsIncr(tv, dtv->counter_erspan);

    return DecodeEthernet(tv, dtv, p, pkt, len, pq);
}

/**
 * \brief ERSPAN Type II
 */
int DecodeERSPANTypeII(ThreadVars *tv, DecodeThreadVars *dtv, Packet *p, const uint8_t *pkt, uint32_t len, PacketQueue *pq)
{
    StatsIncr(tv, dtv->counter_erspan);

    if (len < sizeof(ErspanHdr)) {
        ENGINE_SET_EVENT(p,ERSPAN_HEADER_TOO_SMALL);
        return TM_ECODE_FAILED;
    }
    if (!PacketIncreaseCheckLayers(p)) {
        return TM_ECODE_FAILED;
    }

    const ErspanHdr *ehdr = (const ErspanHdr *)pkt;
    uint16_t version = SCNtohs(ehdr->ver_vlan) >> 12;
    uint16_t vlan_id = SCNtohs(ehdr->ver_vlan) & 0x0fff;

    SCLogDebug("ERSPAN: version %u vlan %u", version, vlan_id);

    /* only v1 is tested at this time */
    if (version != 1) {
        ENGINE_SET_EVENT(p,ERSPAN_UNSUPPORTED_VERSION);
        return TM_ECODE_FAILED;
    }

    if (vlan_id > 0) {
        if (p->vlan_idx >= 2) {
            ENGINE_SET_EVENT(p,ERSPAN_TOO_MANY_VLAN_LAYERS);
            return TM_ECODE_FAILED;
        }
        p->vlan_id[p->vlan_idx] = vlan_id;
        p->vlan_idx++;
    }

    return DecodeEthernet(tv, dtv, p, pkt + sizeof(ErspanHdr), len - sizeof(ErspanHdr), pq);
}

/**
 * @}
 */
