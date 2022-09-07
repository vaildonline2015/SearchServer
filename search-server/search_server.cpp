#include "search_server.h"
#include "log_duration.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cmath>
#include <execution>

using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))  
													 
{
}

SearchServer::SearchServer(const string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))

{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw invalid_argument("Invalid document_id"s);
	}
	const auto words = SplitIntoWordsNoStopAndAddWords(document);

	const double inv_word_count = 1.0 / words.size();
	for (string_view word : words) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		document_to_word_freqs_[document_id][word] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(execution::seq, raw_query, status);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
	return FindTopDocuments(execution::seq, raw_query);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
	return MatchDocument(execution::seq, raw_query, document_id);
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {

	if (document_to_word_freqs_.find(document_id) == document_to_word_freqs_.end()) {
		static map<string_view, double> empty_result;
		return empty_result;
	}
	return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
	RemoveDocument(execution::seq, document_id);
}

bool SearchServer::IsStopWord(std::string_view word) const {
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
	// A valid word must not contain special characters
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}

vector<string_view> SearchServer::SplitIntoWordsNoStopAndAddWords(string_view text) {
	vector<string_view> words;
	for (string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw invalid_argument("Word "s + string(word.data(), word.size()) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(*(words_.insert(string(word))).first);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = 0;
	for (const int rating : ratings) {
		rating_sum += rating;
	}
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
	if (text.empty()) {
		throw invalid_argument("Query word is empty"s);
	}

	bool is_minus = false;
	if (text[0] == '-') {
		is_minus = true;
		text = text.substr(1, text.size() - 1);
	}
	if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
		throw invalid_argument("Query word "s + string(text.data(), text.size()) + " is invalid");
	}

	return { text, is_minus, IsStopWord(text) };
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}