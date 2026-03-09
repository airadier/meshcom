/**
 * MeshCom - ESP-NOW Fragmentation/Reassembly
 * Splits payloads > 240B into fragments with 1-byte header.
 * Separate file so host tests can link without ESP-IDF dependencies.
 */
#include "espnow_comm.h"
#include <string.h>

int espnow_fragment(const uint8_t *data, size_t len,
                    uint8_t frags[][ESPNOW_MAX_DATA], size_t *frag_lens,
                    int max_frags)
{
    /* Each fragment carries 1 header byte + up to (ESPNOW_FRAG_PAYLOAD - 1) payload bytes */
    size_t payload_per_frag = ESPNOW_FRAG_PAYLOAD - 1; /* 239 bytes per fragment */
    int n_frags = (int)((len + payload_per_frag - 1) / payload_per_frag);

    if (n_frags > max_frags || n_frags > 15) return -1; /* 4-bit field max 15 */

    size_t offset = 0;
    for (int i = 0; i < n_frags; i++) {
        size_t chunk = len - offset;
        if (chunk > payload_per_frag) chunk = payload_per_frag;

        /* Header: [frag_index(4 bits) | frag_total(4 bits)] */
        frags[i][0] = (uint8_t)(((i & 0x0F) << 4) | (n_frags & 0x0F));
        memcpy(frags[i] + 1, data + offset, chunk);
        frag_lens[i] = chunk + 1;
        offset += chunk;
    }

    return n_frags;
}

int espnow_reassemble(const uint8_t *frags[], const size_t *frag_lens,
                      int n_frags, uint8_t *out, size_t out_size)
{
    if (n_frags <= 0 || n_frags > ESPNOW_MAX_FRAGS) return -1;

    /* Validate headers: all must agree on total, indices must be 0..total-1 */
    uint8_t expected_total = frags[0][0] & 0x0F;
    if (expected_total != (uint8_t)n_frags) return -1;

    /* Sort by index and validate completeness */
    const uint8_t *sorted[ESPNOW_MAX_FRAGS] = {0};
    size_t sorted_lens[ESPNOW_MAX_FRAGS] = {0};

    for (int i = 0; i < n_frags; i++) {
        if (frag_lens[i] < 2) return -1; /* need at least header + 1 byte */

        uint8_t total = frags[i][0] & 0x0F;
        uint8_t idx   = (frags[i][0] >> 4) & 0x0F;

        if (total != expected_total) return -1;
        if (idx >= expected_total) return -1;
        if (sorted[idx] != NULL) return -1; /* duplicate index */

        sorted[idx] = frags[i];
        sorted_lens[idx] = frag_lens[i];
    }

    /* All indices present? */
    for (int i = 0; i < (int)expected_total; i++) {
        if (sorted[i] == NULL) return -1;
    }

    /* Concatenate payloads */
    size_t total_len = 0;
    for (int i = 0; i < (int)expected_total; i++) {
        size_t payload_len = sorted_lens[i] - 1; /* minus header byte */
        if (total_len + payload_len > out_size) return -1;
        memcpy(out + total_len, sorted[i] + 1, payload_len);
        total_len += payload_len;
    }

    return (int)total_len;
}
