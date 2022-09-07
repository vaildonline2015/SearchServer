#include <vector>
#include <string>

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
	vector<string_view> words;
	size_t offset = 0;
	size_t i = 0;
	bool word_begin = true;

	for (const char c : text) {
		if (c != ' ' && word_begin) {
			offset = i;
			word_begin = false;
		}
		else if (c == ' ' && !word_begin) {
			words.push_back(text.substr(offset, i - offset));
			word_begin = true;
		}
		++i;
	}

	if (!text.empty() && text.back() != ' ') {
		words.push_back(text.substr(offset, i - offset));
	}

	return words;
}
