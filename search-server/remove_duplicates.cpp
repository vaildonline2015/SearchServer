#include <set>
#include <string>
#include <algorithm>

#include "search_server.h"

using namespace std;

bool Equal(const map<string, double>& left, const map<string, double>& right) {
	return left.size() == right.size()
		&& equal(left.begin(), left.end(), right.begin(),
			[](auto a, auto b) { return a.first == b.first; });
}

void RemoveDuplicates(SearchServer& search_server) {

	set<int> duplicates;
	set<string> found_words;

	for (const int document_id : search_server) {

		const auto& word_freqs = search_server.GetWordFrequencies(document_id);

		string query;
		for (const auto& [str, _] : word_freqs) {
			query += string(str.data(), str.size()) + " "s;
		}
		if (found_words.count(query)) {
			duplicates.insert(document_id);
		} 
		else {
			found_words.insert(query);
		}
	}
	for (auto id : duplicates) {
		search_server.RemoveDocument(id);
		cout << "Found duplicate document id "s << id << endl;
	}
}