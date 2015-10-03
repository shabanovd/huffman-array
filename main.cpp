#include <algorithm>
#include <bitset>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include "huffman.h"

template <typename I>
using DifferenceType = typename std::iterator_traits<I>::difference_type;

template <typename I>
// requires ForwardIterator<I>
std::pair<DifferenceType<I>, I> adjacent_count(I first, I last) {
	DifferenceType<I> n{0};
	if (first == last) return std::make_pair(n, first);
	while (std::next(first) != last && *first == *std::next(first)) {
		++n; ++first;
	}
	return std::make_pair(++n, ++first);
}

template <typename I, typename O>
// requires ForwardIterator<I>
// requires OutputIterator<O>
O unique_copy_with_count(I first, I last, O result) {
	while (true) {
		auto count = adjacent_count(first, last);
		if (!count.first) return result;
		*result = std::make_pair(count.first, *first);
		++result;
		first = count.second;
	}
}

template <typename T, typename Op>
// requires Regular<T>
// requires MonoidOpreation<Op, T>
class merge_first_op {
private:
	Op op;
public:
	explicit merge_first_op(const Op& op) : op{op} { }

	template <typename U>
	// requires Regular<U>	
	std::pair<T, U> operator()(const std::pair<T, U>& x, const std::pair<T, U>& y) const {
		return std::make_pair(op(x.first, y.first), U{});
	}
};

template <typename T, typename U, typename Compare>
// requires Regular<T>
// requires Regular<U>
// requires TotalOrdering<Compare, T>
class compare_first : public std::binary_function<std::pair<T, U>, std::pair<T, U>, bool> {
private:
	Compare cmp;
public:
	explicit compare_first(const Compare& cmp) : cmp{cmp} { }

	bool operator()(const std::pair<T, U>& x, const std::pair<T, U>& y) const {
		return cmp(x.first, y.first);
	}
};

template <typename T, typename U>
// requires Regular<T>
// requires Regular<U>
struct get_second {
	const U& operator()(const std::pair<T, U>& x) const {
		return x.second;
	}
};

struct binary_converter {
	template <typename T>
	std::string operator()(const std::pair<T, char>& x) const {
		return std::bitset<8>(x.second).to_string();
	}

	char operator()(const std::string& x) const {
		return static_cast<char>(std::stoi(x, nullptr, 2));
	}
};

std::string compress(const std::string& input) {
	using T = DifferenceType<typename std::string::iterator>;
	using Op = merge_first_op<T, std::plus<T>>;
	using Compare = compare_first<T, char, std::less<T>>;

	std::string x = input;
	std::sort(x.begin(), x.end());

	std::vector<std::pair<T, char>> frequencies;
	unique_copy_with_count(x.begin(), x.end(), std::back_inserter(frequencies));

	Op op{std::plus<T>{}};
	Compare cmp{std::less<T>{}};

	std::sort(frequencies.begin(), frequencies.end(), cmp);
	huffman_encoder<std::pair<T, char>, Compare, Op> encoder{frequencies, cmp, op};
	
	return encoder(input.begin(), input.end(), get_second<T, char>{}, binary_converter{});
}

std::string decompress(const std::string& input) {
	huffman_decoder<char> decoder;
	std::string result;
	decoder(input, std::back_inserter(result), binary_converter{});
	return result;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cout << "expected one argument\n";
		return 1;
	}

	std::cout << "--Input Message--\n";
	std::string str{argv[1]};
	std::cout << str << std::endl;

	std::cout << "\n--Compressed Message--\n";
	std::string compressed = compress(str);
	std::cout << compressed << std::endl;

	std::cout << "\n--Decompressed Message--\n";
	std::string result = decompress(compressed);
	std::cout << result << std::endl;

	std::cout << "\n--Compression Results--\n";
	std::cout << "Input Size: " << str.size() * 8 << " bits\n";
	std::cout << "Output Size (including header): " << compressed.size() << " bits\n";
}

