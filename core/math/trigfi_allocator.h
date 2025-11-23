#include <vector>
#include "fint.h"
#include "vector2fi.h"

//Deterministic trigonometry allocator
namespace DtrmnTrigAllocator {
    static bool initialized = false;
    const static int start_amount = 512;
    const static int increment = 256;
    static std::vector<TrigMemoryBlock> free_index;
    static std::vector<FInt> data;

    constexpr static FInt* allocate_vector2s(uint32_t vector_amount) {
        DtrmnTrigAllocator::allocate_numbers(vector_amount * 2);
    }

    constexpr static FInt* allocate_numbers(uint32_t number_amount) {
        
    }

    constexpr static void ensure_space(uint32_t amount)
    {
        if(unlikely(!initialized))
        {
            int real_start_amount = start_amount;
            if(amount > start_amount)
            {
                int amount_mod = amount % increment;
                real_start_amount = amount - amount_mod;
                amount_mod += amount_mod > 0 ? increment : 0;
            }

            data.reserve(real_start_amount);
            free_index.push_back(TrigMemoryBlock(0, real_start_amount));

            initialized = true;

            return;
        }
    }

    struct TrigMemoryBlock
    {
        int index;
        int length;

        constexpr TrigMemoryBlock(int idx, int len) : index(idx), length(len) {}
    };
    
}
