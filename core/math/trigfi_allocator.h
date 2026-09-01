#include "fint.h"
#include "vector2fi.h"
#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

//TODO: complete this.

//Deterministic trigonometry allocator
/*
namespace DtrmnTrigAllocator {

	static bool initialized = false;
	const static int start_amount = 512;
	const static int increment = 256;
	static std::vector<TrigMemoryBlock> free_index;
	static std::vector<FInt> data;

	constexpr static FInt* allocate_vector2s(uint32_t vector_amount) {
		DtrmnTrigAllocator::allocate_numbers(vector_amount * 2);
	}

	constexpr static TrigMemoryBlock allocate_numbers(uint32_t number_amount) {
		ensure_initialized(number_amount);

		//try to get memory in the pool
		TrigMemoryBlock data = retrieve_data(number_amount);

		if(data.length == 0)
		{
			//if no memory, do default allocation by increment
			//and retrieve a piece with the size desired.
			data = d_allocate_and_retrieve_data(number_amount);
		}

		return data;
	}

	constexpr static void ensure_initialized(uint32_t amount)
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

	constexpr static TrigMemoryBlock retrieve_data(uint32_t amount)
	{

	}

	constexpr static TrigMemoryBlock d_allocate_and_retrieve_data(uint32_t amount)
	{

	}

	struct TrigMemoryBlock
	{
		public:

		uint32_t index;
		uint32_t length;

		FInt& operator[](int idx)
		{
			return TrigMemoryBlock::get(idx);
		}

		constexpr TrigMemoryBlock(uint32_t idx, uint32_t len) : index(idx), length(len) {}

		constexpr FInt& get(int idx)
		{
			if(unlikely(idx < 0 || index >= length))
			{
				throw_idx(idx);
			}

			return DtrmnTrigAllocator::data[index + idx];
		}

		constexpr Vector2FI get_vec2(int idx) { return Vector2FI(this->get(idx << 1), this->get((idx << 1)+1)); }
		constexpr void set_vec2(int idx, Vector2FI value) {
			this->get(idx << 1) = value.x;
			this->get((idx << 1)+1) = value.y;
		}

		constexpr TrigMemoryBlock take_piece_start(uint32_t amount)
		{
			if(unlikely(amount > length))
				throw_split(amount);

			uint32_t result_idx = index;
			uint32_t result_len = amount;

			index += amount;
			length -= amount;

			return TrigMemoryBlock(result_idx, result_len);
		}

		constexpr TrigMemoryBlock take_piece_end(uint32_t amount)
		{
			if(unlikely(amount > length))
				throw_split(amount);

			uint32_t result_idx = length - amount;
			uint32_t result_len = amount;

			length -= amount;

			return TrigMemoryBlock(result_idx, result_len);
		}

		private:

		constexpr void throw_idx(int idx)
		{
			throw std::out_of_range(
				"Index "
				+ std::to_string(idx)
				+ " out of range in trigonometry memory block["
				+ std::to_string(index)
				+ ", "
				+ std::to_string(length)
				+ "]."
			);
		}

		constexpr void throw_split(int amount)
		{
			throw std::out_of_range("Tried to split "
				+ std::to_string(amount)
				+ " out of a memory block of "
				+ std::to_string(length)
			);
		}
	};

}
*/