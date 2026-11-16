#include "qrcodegen.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    const char *challenge = "https://s.team/q/1/0123456789ABCDEF";
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(4)] = {0};
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(4)] = {0};
    assert(qrcodegen_encodeText(challenge, tmp, qr, qrcodegen_Ecc_LOW,
                               qrcodegen_VERSION_MIN, 4,
                               qrcodegen_Mask_AUTO, false));
    const int size = qrcodegen_getSize(qr);
    assert(size >= 21 && size <= 33);
    // The top-left finder pattern should have a 7x7 dark outline.
    for (int i = 0; i < 7; ++i) {
        assert(qrcodegen_getModule(qr, i, 0));
        assert(qrcodegen_getModule(qr, 0, i));
    }
    assert(!qrcodegen_getModule(qr, -1, -1)); // quiet margin
    puts("Local Steam Guard QR generated");
    return 0;
}
