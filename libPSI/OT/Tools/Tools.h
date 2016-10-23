#pragma once
 
#include "Common/Defines.h"

namespace osuCrypto {



    void mul128(block x, block y, block &xy1 , block &xy2);

    void eklundh_transpose128(std::array<block, 128>& inOut);
    void sse_transpose128(std::array<block, 128>& inOut);
    void print(std::array<block, 128>& inOut);
    u8 getBit(std::array<block, 128>& inOut, u64 i, u64 j);
}
