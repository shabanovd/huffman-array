#pragma once

#include <bitset>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename I>
using ValueType = typename std::iterator_traits<I>::value_type;

template <typename I, typename Compare>
// requires ForwardIterator<I>
// requires BinaryPredicate<Compare>
I next_node(I& f0, I l0, I& f1, I l1, Compare cmp) {
	if (f1 == l1) return f0++;
	if (f0 == l0) return f1++;
	if (cmp(*f1, *f0)) return f1++;
	return f0++;
}

template <typename I, typename Compare, typename F>
// requires ForwardIterator<I>
// requires TotalOrdering<Compare, ValueType<I>>
// requires UnaryFunction<F, std::pair<I, std::string>>
void generate_codes(I f0, I l0, I f1, I l1, Compare cmp, F f) {
	std::vector<std::pair<I, std::string>> prefixes;
	auto n = l0 - f1;
	prefixes.reserve(n);
	
	// Add the 'root' element
	prefixes.emplace_back(f1, std::string{});
	++f1;
	auto current = prefixes.begin();

	while (n) {
		--n;
		// is this a 'leaf node'?
		if (current->first >= l1) {
			f(*current);
			++current;
			continue;
		}
		auto x = next_node(f0, l0, f1, l1, cmp);
		prefixes.emplace_back(x, current->second + '1');
		auto y = next_node(f0, l0, f1, l1, cmp);
		prefixes.emplace_back(y, current->second + '0');
		++current;
	}
}

template <typename T, typename Compare, typename Op>
// requires Regular<T>
// requires TotalOrdering<Compare, T>
// requires MonoidOperation<Op, T>
class huffman_encoder {
private:
	std::vector<T> nodes;
	Compare cmp;
	Op op;
public:
	huffman_encoder(std::vector<T> nodes, const Compare& cmp, const Op& op) : nodes{std::move(nodes)}, cmp{cmp}, op{op} { 
		// precondition: is_sorted(nodes.begin(), nodes.end(), cmp)
	}

	template <typename I, typename F, typename BinaryConverter>
	std::string operator()(I first, I last, F f, BinaryConverter converter) {
		using reverse_iterator = typename std::vector<T>::reverse_iterator;
		auto lnodes = nodes.size();
		build_huffman_array();
		
		std::string result = header(converter);
		std::unordered_map<ValueType<I>, std::string> st;
		auto st_op = [&st, f](const std::pair<reverse_iterator, std::string>& x) {
			st.insert(std::make_pair(f(*x.first), x.second));
		};

		generate_codes(nodes.rend() - lnodes, nodes.rend(), nodes.rbegin(), nodes.rend() - lnodes, std::not2(cmp), st_op);
		
		// encode the input with generated codes
		while (first != last) {
			result += st[*first];
			++first;
		}

		return result;
	}

private:
	void build_huffman_array() {
		auto size = nodes.size();
		auto n = size * 2 - 1; // huffman tree with {size} nodes has {size} * 2 - 1 total nodes
		nodes.reserve(n); // important, we don't want to invalidate iterators adding elements later

		auto f0 = nodes.begin() + 2; // skip first two elements since they are the smallest
		auto l0 = nodes.end();
		nodes.push_back(op(nodes[0], nodes[1]));

		auto f1 = l0;
		auto l1 = nodes.begin() + n;

		while (nodes.end() != l1) {
			auto x = next_node(f0, l0, f1, nodes.end(), cmp); // smallest
			auto y = next_node(f0, l0, f1, nodes.end(), cmp); // next smallest
			nodes.push_back(op(*x, *y));
		}
	}

	template <typename BinaryConverter>
	std::string header(BinaryConverter converter) {
		std::bitset<16> size{nodes.size()};
		std::string result = size.to_string();

		auto f0 = nodes.begin();
		auto l0 = nodes.begin() + nodes.size() / 2 + 1; // end of leaf nodes
		auto f1 = l0; // start of internal nodes
		// traverse huffman array in sorted order of frequency
		while (f0 != l0	|| f1 != nodes.end()) {
			auto x = next_node(f0, l0, f1, nodes.end(), cmp);
			// is leaf node
			if (x < l0) {
				result += '1';
				result += converter(*x);
			} else {
				result += '0';
			}
		}
		return result;
	}
};

template <typename T>
// requires Regular<T>
class huffman_decoder {
private:
	std::vector<std::pair<int, T>> nodes;
public:
	template <typename O, typename BinaryConverter>
	// requires OutputIterator<I>
	O operator()(const std::string& input, O result, BinaryConverter converter) {
		using reverse_iterator = typename std::vector<std::pair<int, T>>::reverse_iterator;
		auto current = read_header(input, converter);	
		auto lnodes = nodes.size() / 2 + 1;
		// keep hold of the prefixes for the leaf nodes
		std::vector<std::pair<reverse_iterator, std::string>> prefixes;
		prefixes.reserve(lnodes);
		auto prefix_op = [&prefixes](const std::pair<reverse_iterator, std::string>& x) {
			prefixes.push_back(x);
		};

		std::sort(prefixes.begin(), prefixes.end(), [](const std::pair<reverse_iterator, std::string>& x, const std::pair<reverse_iterator, std::string>& y) {
			return x.second.size() < y.second.size();
		});
			
		auto cmp = [](const std::pair<int, T>& x, const std::pair<int, T>& y) { return !(x.first < y.first); };
		generate_codes(nodes.rend() - lnodes, nodes.rend(), nodes.rbegin(), nodes.rend() - lnodes, cmp, prefix_op);

		std::string bits_read;
		while (current != input.end()) {
			// find the matching prefix
			for (const auto& prefix : prefixes) {
				// read bits until we match the current prefix size
				while (prefix.second.size() > bits_read.size()) {
					bits_read.push_back(*current);
					++current;
				}

				if (bits_read == prefix.second) {
					*result = prefix.first->second;
					++result;
					bits_read.clear(); // make sure to reset the bits read
					break;
				}
			}
		}
		return result;
	}

private:
	template <typename BinaryConverter>
	typename std::string::const_iterator read_header(const std::string& input, BinaryConverter converter) {
		std::bitset<16> size{input.substr(0, 16)};
		nodes = std::vector<std::pair<int, T>>(size.to_ulong());
		auto lnodes = 0;
		auto inodes = nodes.size() / 2 + 1;
		auto first = input.begin() + 16;

		for (unsigned i = 0; i < nodes.size(); ++i) {
			T x;
			bool isleaf = *first == '1';
			++first;
			if (isleaf) {
				x = converter(input.substr(first - input.begin(), sizeof(T) * 8));
				first += sizeof(T) * 8;
				nodes[lnodes++] = std::make_pair(i, x);
			} else {
				nodes[inodes++] = std::make_pair(i, x);
			}
		}
		return first;
	}
};



