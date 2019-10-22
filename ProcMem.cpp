#include <iostream>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <string>

uint64_t hexToNumber (const std::string& str) {
	uint64_t acc = 0;

	for (size_t i = 0; isxdigit(static_cast<unsigned char>(str[i])); i++) {
		char c = str[i];
		acc *= 16;
		if (isdigit(c)) {
			acc += static_cast<uint64_t>(c - '0');
		} else if (isupper(c)) {
			acc += static_cast<uint64_t>(c - 'A' + 10);
		} else {
			acc += static_cast<uint64_t>(c - 'a' + 10);
		}
	}

	return acc;
}

struct Permissions {
	bool read = false;
	bool write = false;
	bool execute = false;
	bool priv = false;
	bool shared = false;
};

struct MemEntry {
	uint64_t address_start;
	uint64_t address_end;
	uint64_t address_size;

	Permissions perms;
	uint64_t offset;

	std::string dev;

	std::string inode;

	std::string pathname;

	MemEntry () {}
};
class MemMap {
	public:
	std::string code;
	std::vector<MemEntry> entries;

	explicit MemMap (std::string t_code) : code(t_code) {
		parse();
	}

	void parse () {
		size_t position = 0;


		while (position < code.size()) {
			MemEntry entry;

			entry.address_start = parseAddress(position);

			if (code.at(position) != '-') {
				throw std::runtime_error("Invalid map, expected - after offset at position: " + std::to_string(position));
			}
			position++;

			entry.address_end = parseAddress(position);
			entry.address_size = entry.address_end - entry.address_start;

			position++; // skip space

			if (code.at(position) == 'r') {
				entry.perms.read = true;
			}
			position++;
			if (code.at(position) == 'w') {
				entry.perms.write = true;
			}
			position++;
			if (code.at(position) == 'x') {
				entry.perms.execute = true;
			}
			position++;
			if (code.at(position) == 'p') {
				entry.perms.priv = true;
			} else if (code.at(position) == 's') {
				entry.perms.shared = true;
			}
			position++;

			position++; // skip space

			entry.offset = parseOffset(position);

			position++; // skip space

			entry.dev = parseDev(position);

			position++; // skip space

			entry.inode = parseInode(position);

			consumeSpaces(position);

			entry.pathname = parsePathname(position);

			if (position < code.size()) {
				if (code.at(position) == '\n' || code.at(position) == '\r') {
					position++;
					if (position < code.size()) {
						if (code.at(position) == '\n' || code.at(position) == '\r') {
							position++;
						}
					}
				}
			}

			entries.push_back(entry);

			if (position >= code.size()) break;
		}
	}

	void consumeSpaces (size_t& position) {
		while (position < code.size() && code.at(position) == ' ') {
			position++;
		}
	}

	uint64_t parseAddress (size_t& position) {
		std::string text = parseTextHex(position);
		return hexToNumber(text);
	}

	uint64_t parseOffset (size_t& position) {
		std::string text = parseTextHex(position);
		return hexToNumber(text);
	}

	std::string parseDev (size_t& position) {
		return parseText(position, false);
	}

	std::string parseInode (size_t& position) {
		return parseText(position, false);
	}

	std::string parsePathname (size_t& position) {
		return parseText(position, true);
	}

	std::string parseTextHex (size_t& position) {
		std::string res;
		while (position < code.size()) {
			char c = code.at(position);
			if (c == '\n' || c == '\r' || c == '\0' || c == ' ' || c == '-') {
				break;
			}
			res += c;
			position++;
		}
		return res;
	}

	std::string parseText (size_t& position, bool allow_spaces=false) {
		std::string res;

		while (position < code.size()) {
			char c = code.at(position);
			if (c == '\n' || c == '\r' || c == '\0' || (!allow_spaces && c == ' ')) {
				break;
			}
			res += c;
			position++;
		}

		return res;
	}
};

void scanHeap (std::fstream& file, MemEntry& heap) {
	std::cout << "Reading heap\n";
	std::cout << "Heap size: " << heap.address_size << "\n";

	// Create a vector the size of the heaps
	std::vector<char> data;
	data.resize(heap.address_size);

	// Seek to position
	file.seekg(heap.address_start);
	// Read data into vector
	file.read(data.data(), heap.address_size);

	if (file.fail()) {
		std::cout << "Failed to read heap :(\n";
		file.clear();
		return;
	}

	std::cout << "Success\n";
	std::cout << "Heap-Vector size: " << data.size() << ", Equal:" << ((heap.address_size == data.size()) ? "Yes" : "No") << "\n";

	for (size_t i = 0; i < data.size(); i++) {
		if (data.size() - i <= 26) {
			break;
		}

		// Scan for first four letters of alphabet
		if (
			data.at(i) == 'a' && data.at(i + 1) == 'b' && data.at(i + 2) == 'c' && data.at(i + 3) == 'd'
		) {
			std::cout << "Found position: " << i << "\n";

			if (i + 1024 > heap.address_size) {
				std::cout << ".. But it seems to go out of bounds?\n";
				return;
			}

			std::cout << "Text:\n";
			for (size_t j = 0; j < 1024; j++) {
				std::cout << data.at(i + j);
			}
			std::cout << "\n\n";

			file.seekp(heap.address_start + i);
			file.put('Z');

			break;
		}
	}
}

int main () {
	char* alpha = (char*)malloc(1024);
	for (int i = 0; i < 1024; i++) {
		*(alpha+i) = (char) ('a' + i % 26);
	}

	std::fstream file;
	file.open("/proc/self/mem");

	if (file.fail()) {
		std::cout << "FAILED TO OPEN FILE\n";
		return 1;
	}
	std::cout << "Opened file\n";



	std::ifstream maps_file;
	maps_file.open("/proc/self/maps", std::ios_base::in);

	if (maps_file.fail()) {
		std::cout << "FAILED TO OPEN MAP\n";
		return 1;
	}

	std::string maps_text(
		(std::istreambuf_iterator<char>(maps_file)),
		(std::istreambuf_iterator<char>())
	);
	std::cout << maps_text << "\n";

	// Parse memory maps
	MemMap maps = MemMap(maps_text);

	// Find the heap map entry
	for (MemEntry& entry : maps.entries) {
		std::cout << "Entry: " << entry.address_start << " [" << entry.address_size << "] '" << entry.pathname << "'\n";
		if (entry.pathname == "[heap]") {
			if (entry.address_size == 0) {
				std::cout << "Heap had size of 0..\n";
			} else {
				scanHeap(file, entry);
				break;
			}
		}
	}



	maps_file.close();
	file.close();

	// Print out text
	for (int i = 0; i < 1024; i++) {
		std::cout << *(alpha+i);
	}
	std::cout << "\n";

	return 0;
}